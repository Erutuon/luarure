#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <rure.h> // stdbool.h included here

#define RURE_NAME "rure"
#define RURE_CAPTURES_NAME "rure_captures"
#define RURE_ITER_NAME "rure_iter"

#define CHECK_RURE(L, i) (* (rure * *) luaL_checkudata((L), (i), RURE_NAME))

#define PUSH_MATCH(L, str, match) \
	lua_pushlstring(L, &str[match.start], match.end - match.start)

#define NEW_USERDATA(L, type, val, name) \
	do { \
		type * * ud = lua_newuserdata(L, sizeof *ud); \
		*ud = val; \
		luaL_setmetatable(L, name); \
	} while (0)

#define MAKE_FUNCS(type, name) \
	static inline type * lua_check_##type(lua_State * L, int i) { \
		type * * ud = luaL_checkudata(L, i, name); \
		luaL_argcheck(L, *ud != NULL, i, "attempt to use a freed " name); \
		return *ud; \
	} \
	static int lua##type##_gc (lua_State * L) { \
		type * * ud = luaL_checkudata(L, 1, name); \
		type * var = *ud; \
		if (var != NULL) { \
			type##_free(var); \
			*ud = NULL; \
		} \
		return 0; \
	}

MAKE_FUNCS(rure, RURE_NAME)

MAKE_FUNCS(rure_captures, RURE_CAPTURES_NAME)

MAKE_FUNCS(rure_iter, RURE_ITER_NAME)

#undef MAKE_FUNCS

// rure captures methods
static int luarure_captures_index (lua_State * L) {
	rure_captures * captures = lua_check_rure_captures(L, 1);
	rure_match match;
	const char * str;
	int32_t index;
	
	switch (lua_type(L, 2)) {
		case LUA_TNUMBER: {
			if (lua_isinteger(L, 2)) {
				lua_Integer supplied_index = lua_tointeger(L, 2);
				
				if (0 <= supplied_index && supplied_index <= INT32_MAX
				&& lua_getuservalue(L, 1) == LUA_TTABLE) {
					index = (int32_t) supplied_index;
					goto push_match;
				}
			}
			break;
		}
		case LUA_TSTRING: {
			if (lua_getuservalue(L, 1) == LUA_TTABLE
			&& lua_getfield(L, -1, "regex") == LUA_TUSERDATA) {
				rure * regex = lua_check_rure(L, -1);
				const char * name = lua_tostring(L, 2);
				index = rure_capture_name_index(regex, name);
				
				if (index >= 0) {
					lua_pop(L, 1);
					goto push_match;
				}
			}
			break;
		}
		default: (void) (0);
	}
	
push_match:
	if (rure_captures_at(captures, (size_t) index, &match)
	&& lua_getfield(L, -1, "haystack") == LUA_TSTRING) {
		str = lua_tostring(L, -1);
		PUSH_MATCH(L, str, match);
	} else
		lua_pushnil(L);
	
	return 1;
}

static int luarure_captures_len (lua_State * L) {
	rure_captures * captures = lua_check_rure_captures(L, 1);
	lua_pushinteger(L, (lua_Integer) rure_captures_len(captures));
	return 1;
}

// rure constructor and metamethods
static int luarure_new (lua_State * L) {
	size_t len;
	const char * re = luaL_checklstring(L, 1, &len);
	rure_error * error = rure_error_new();
	rure * regex = rure_compile((const uint8_t *) re, len, RURE_DEFAULT_FLAGS, NULL, error);
	
	if (regex == NULL) {
		const char * msg = lua_pushstring(L, rure_error_message(error));
		rure_error_free(error);
		return luaL_argerror(L, 1, msg);
	}
	
	rure_error_free(error);
	
	NEW_USERDATA(L, rure, regex, RURE_NAME);
	
	return 1;
}

// rure methods
static int luarure_is_match (lua_State * L) {
	rure * regex = lua_check_rure(L, 1);
	size_t len;
	const char * str = luaL_checklstring(L, 2, &len);
	size_t start = luaL_optinteger(L, 3, 1) - 1; // adjust index
	bool res = rure_is_match(regex, (const uint8_t *) str, len, start);
	lua_pushboolean(L, res);
	return 1;
}

static int luarure_find (lua_State * L) {
	rure * regex = lua_check_rure(L, 1);
	size_t len;
	const char * str = luaL_checklstring(L, 2, &len);
	size_t start = luaL_optinteger(L, 3, 1) - 1; // adjust index
	rure_match match;
	if (rure_find(regex, (const uint8_t *) str, len, start, &match))
		PUSH_MATCH(L, str, match);
	else
		lua_pushnil(L);
	return 1;
}

#define ADJUST_NEGATIVE_INDEX_BY(index, val) \
	((index) < 0 ? (index) + (val) : (index))

static inline void luarure_add_uservalue (lua_State * L,
											 int regex_index,
											 int haystack_index) {
	lua_createtable(L, 0, 2);
	regex_index = ADJUST_NEGATIVE_INDEX_BY(regex_index, -1);
	haystack_index = ADJUST_NEGATIVE_INDEX_BY(haystack_index, -1);
#define PUSH_SET(L, t, k, v) (lua_pushvalue((L), (v)), lua_setfield((L), (t) - 1, (k)))
	PUSH_SET(L, -1, "regex", regex_index);
	PUSH_SET(L, -1, "haystack", haystack_index);
#undef PUSH_SET
	lua_setuservalue(L, -2); // Set as uservalue of captures.
}

static inline void lua_push_rure_captures (lua_State * L,
                                           rure_captures * captures,
                                           int regex_index,
                                           int haystack_index) {
	NEW_USERDATA(L, rure_captures, captures, RURE_CAPTURES_NAME);
	regex_index = ADJUST_NEGATIVE_INDEX_BY(regex_index, -1);
	haystack_index = ADJUST_NEGATIVE_INDEX_BY(haystack_index, -1);
	luarure_add_uservalue(L, regex_index, haystack_index);
}

#undef ADJUST_NEGATIVE_INDEX_BY

static int luarure_find_captures (lua_State * L) {
	rure * regex = lua_check_rure(L, 1);
	size_t len;
	const char * str = luaL_checklstring(L, 2, &len);
	size_t start = luaL_optinteger(L, 3, 1) - 1; // adjust index
	rure_captures * captures = rure_captures_new(regex);
	if (rure_find_captures(regex, (const uint8_t *) str, len, start, captures))
		lua_push_rure_captures(L, captures, 1, 2);
	else
		lua_pushnil(L);
	return 1;
}


static int luarure_iter_shared (lua_State * L, int (* func) (lua_State *)) {
	rure * regex = lua_check_rure(L, 1);
	luaL_checkstring(L, 2);
	rure_iter * iter = rure_iter_new(regex);
	
	lua_pushcfunction(L, func);
	
	NEW_USERDATA(L, rure_iter, iter, RURE_ITER_NAME);
	luarure_add_uservalue(L, 1, 2);
	
	return 2;
}

static int luarure_iter_next (lua_State * L);
static int luarure_iter_next_captures (lua_State * L);

static int luarure_iter (lua_State * L) {
	return luarure_iter_shared(L, luarure_iter_next);
}

static int luarure_iter_captures (lua_State * L) {
	return luarure_iter_shared(L, luarure_iter_next_captures);
}

// rure_iter methods
static int luarure_iter_next (lua_State * L) {
	rure_iter * iter = lua_check_rure_iter(L, 1);
	if (lua_getuservalue(L, 1) == LUA_TTABLE
	&& lua_getfield(L, -1, "haystack") == LUA_TSTRING) {
		size_t len;
		const char * str = lua_tolstring(L, -1, &len);
		if (str != NULL) {
			rure_match match;
			if (rure_iter_next(iter, (const uint8_t *) str, len, &match)) {
				PUSH_MATCH(L, str, match);
				return 1;
			}
		}
	}
	return 0;
}

static int luarure_iter_next_captures (lua_State * L) {
	rure_iter * iter = lua_check_rure_iter(L, 1);
	if (lua_getuservalue(L, 1) == LUA_TTABLE
	&& lua_getfield(L, -1, "haystack") == LUA_TSTRING) {
		size_t len;
		const char * str = lua_tolstring(L, -1, &len);
		if (str != NULL
		&& lua_getfield(L, -2, "regex") == LUA_TUSERDATA) {
			rure * regex = lua_check_rure(L, -1);
			rure_captures * captures = rure_captures_new(regex);
			if (rure_iter_next_captures(iter, (const uint8_t *) str, len, captures)) {
				lua_push_rure_captures(L, captures, -1, -2);
				return 1;
			}
			rure_captures_free(captures);
		}
	}
	return 0;
}

#define ENTRY(name) { #name, luarure_##name }

static const luaL_Reg luarure_funcs[] = {
	ENTRY(new),
	ENTRY(iter),
	{ NULL, NULL }
};

static const luaL_Reg luarure_metamethods[] = {
	{ "__gc", luarure_gc },
	{ NULL, NULL }
};

static const luaL_Reg luarure_methods[] = {
	ENTRY(is_match),
	ENTRY(find),
	ENTRY(find_captures),
	ENTRY(iter),
	ENTRY(iter_captures),
	{ NULL, NULL }
};

#undef ENTRY

static const luaL_Reg luarure_captures_metamethods[] = {
	{ "__gc", luarure_captures_gc },
	{ "__index", luarure_captures_index },
	{ "__len", luarure_captures_len },
	{ NULL, NULL }
};

static const luaL_Reg luarure_iter_metamethods[] = {
	{ "__gc", luarure_iter_gc },
	{ NULL, NULL }
};

#define NEW_METATABLE(L, name, funcs) \
	luaL_newmetatable(L, name), \
	luaL_setfuncs(L, funcs, 0)

int luaopen_luarure (lua_State * L) {
	NEW_METATABLE(L, RURE_NAME, luarure_metamethods);
	luaL_newlib(L, luarure_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	
	NEW_METATABLE(L, RURE_CAPTURES_NAME, luarure_captures_metamethods);
	lua_pop(L, 1);
	
	NEW_METATABLE(L, RURE_ITER_NAME, luarure_iter_metamethods);
	lua_pop(L, 1);
	
	luaL_newlib(L, luarure_funcs);
	
	return 1;
}

#undef NEW_METATABLE