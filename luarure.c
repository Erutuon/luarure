#include <stdio.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <rure.h> // stdbool.h included here

// https://stackoverflow.com/a/3213261
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#define ARRAY_SIZE(array) (sizeof (array) / sizeof ((array)[0]))

#define RURE_NAME "rure"
#define RURE_CAPTURES_NAME "rure_captures"
#define RURE_ITER_NAME "rure_iter"

#define FREED_ERROR "attempt to use a freed "

#define PUSH_MATCH(L, str, match) \
	lua_pushlstring(L, &str[match.start], match.end - match.start)

#define NEW_USERDATA(L, type, val, name) \
	do { \
		type * * ud = lua_newuserdata(L, sizeof *ud); \
		*ud = val; \
		luaL_setmetatable(L, name); \
	} while (0)

#define MAKE_FUNCS(type, name) \
	static inline type * lua_check_##type##_meta(lua_State * L, \
                                          int i, \
                                          bool allow_freed) { \
		type * * ud = luaL_checkudata(L, i, name); \
		if (!allow_freed) \
			luaL_argcheck(L, *ud != NULL, i, \
                          FREED_ERROR name); \
		return *ud; \
	} \
	static inline type * lua_check_##type(lua_State * L, \
                                          int i) { \
		return lua_check_##type##_meta(L, i, false); \
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

static int luarure_captures_push_table (lua_State * L) {
	rure_captures * captures = lua_check_rure_captures(L, 1);
	size_t captures_len;
	rure * regex;
	rure_iter_capture_names * iter;
	char * capture_name;
	
	lua_settop(L, 1);
	
	if (!(lua_getuservalue(L, 1) == LUA_TTABLE
	&& lua_getfield(L, 2, "haystack") == LUA_TSTRING
	&& lua_getfield(L, 2, "regex") == LUA_TUSERDATA))
		return luaL_argerror(L, 1, "invalid uservalue in " RURE_CAPTURES_NAME);
	
	captures_len = rure_captures_len(captures);
	lua_createtable(L, captures_len, 0);
	
	if (captures_len == 0)
		return 1;
	
	const char * haystack = lua_tostring(L, 3);
	regex = lua_check_rure(L, 4);
	
	for (size_t i = 0; i < captures_len; ++i) {
		rure_match match;
		if (rure_captures_at(captures, i, &match)) {
			PUSH_MATCH(L, haystack, match);
			lua_seti(L, 5, i);
		}
	}
	
	iter = rure_iter_capture_names_new(regex);
	while (rure_iter_capture_names_next(iter, &capture_name)) {
		int32_t index = rure_capture_name_index(regex, capture_name);
		rure_match match;
		if (index != -1 && rure_captures_at(captures, index, &match)) {
			PUSH_MATCH(L, haystack, match);
			lua_setfield(L, 5, capture_name);
		}
	}
	rure_iter_capture_names_free(iter);
	
	return 1;
}

// rure captures methods
static int luarure_captures_index (lua_State * L) {
	rure_captures * captures = lua_check_rure_captures_meta(L, 1, true);
	rure_match match;
	const char * str;
	int32_t index;
	
	switch (lua_type(L, 2)) {
		case LUA_TNUMBER: {
			if (lua_isinteger(L, 2)) {
				lua_Integer supplied_index = lua_tointeger(L, 2);
				
				luaL_argcheck(L, captures != NULL, 1,
                              FREED_ERROR RURE_CAPTURES_NAME);
				
				if (0 <= supplied_index && supplied_index <= INT32_MAX
				&& lua_getuservalue(L, 1) == LUA_TTABLE) {
					index = (int32_t) supplied_index;
					goto push_match;
				}
			}
			break;
		}
		case LUA_TSTRING: {
			size_t len;
			const char * key = lua_tolstring(L, 2, &len);
			
			if (len == sizeof "to_table" - 1
			&& strcmp(key, "to_table") == 0) {
				lua_pushcfunction(L, luarure_captures_push_table);
				return 1;
			}
			
			luaL_argcheck(L, captures != NULL, 1,
						  FREED_ERROR RURE_CAPTURES_NAME);
			
			if (lua_getuservalue(L, 1) == LUA_TTABLE
			&& lua_getfield(L, -1, "regex") == LUA_TUSERDATA) {
				rure * regex = lua_check_rure(L, -1);
				const char * name = lua_tostring(L, 2);
				index = rure_capture_name_index(regex, name);
				
				if (index != -1) {
					lua_pop(L, 1); // Ensure uservalue is at index -1.
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
	// Subtract 1 to give the length that captures would have if it were
	// converted to a Lua table.
	lua_pushinteger(L, (lua_Integer) rure_captures_len(captures) - 1);
	return 1;
}

// This must be kept in sync with the RURE_FLAG_ macros
// in rure.h.
static const char * luarure_flags[] = {
	"CASEI",
	"MULTI",
	"DOTNL",
	"SWAP_GREED",
	"SPACE",
	"UNICODE",
	NULL
};

#define MAX_FLAG (sizeof "SWAP_GREED")
#define INVALID_FLAG ((uint32_t) -1)

static inline uint32_t luarure_get_flag_value (const char * arg) {
	for (int i = 0; luarure_flags[i] != NULL; ++i) {
		if (strcasecmp(arg, luarure_flags[i]) == 0)
			return 1 << i;
	}
	
	return INVALID_FLAG;
}

static inline uint32_t luarure_check_flags (lua_State * L,
											int start,
											uint32_t def) {
	uint32_t flags = 0;
	int top = lua_gettop(L);
	
	for (int i = start; i <= top; ++i) {
		size_t len;
		const char * arg = luaL_checklstring(L, i, &len);
		
		if (len <= MAX_FLAG) {
			uint32_t flag = luarure_get_flag_value(arg);
			
			if (flag != INVALID_FLAG) {
				if (flags & flag)
					return luaL_argerror(L, i, "the same flag was provided twice");
				else {
					flags |= flag;
					continue;
				}
			}
		}
		
		return luaL_argerror(L, i, "invalid flag");
	}
	
	return flags == 0 ? def : flags;
}

// rure constructor and metamethods
static int luarure_new (lua_State * L) {
	size_t len;
	const char * re = luaL_checklstring(L, 1, &len);
	uint32_t flags = luarure_check_flags(L, 2, RURE_DEFAULT_FLAGS);
	rure_error * error = rure_error_new();
	rure * regex = rure_compile((const uint8_t *) re, len, flags, NULL, error);
	
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

static const luaL_Reg luarure_iter_methods[] = {
	{ "next", luarure_iter_next },
	{ "next_captures", luarure_iter_next_captures },
	{ NULL, NULL }
};

static void luarure_push_flag_array (lua_State * L) {
	lua_createtable(L, 0, ARRAY_SIZE(luarure_flags) - 1);
	for (int i = 0; luarure_flags[i] != NULL; ++i) {
		lua_pushstring(L, luarure_flags[i]);
		lua_seti(L, -2, i);
	}
}

#define NEW_METATABLE(L, name, funcs) \
	luaL_newmetatable(L, name), \
	luaL_setfuncs(L, funcs, 0)

#define ADD_INDEX_METATABLE(L, funcs) \
	luaL_newlib(L, funcs), \
	lua_setfield(L, -2, "__index");

int luaopen_luarure (lua_State * L) {
	NEW_METATABLE(L, RURE_NAME, luarure_metamethods);
	ADD_INDEX_METATABLE(L, luarure_methods);
	lua_pop(L, 1);
	
	NEW_METATABLE(L, RURE_CAPTURES_NAME, luarure_captures_metamethods);
	lua_pop(L, 1);
	
	NEW_METATABLE(L, RURE_ITER_NAME, luarure_iter_metamethods);
	ADD_INDEX_METATABLE(L, luarure_iter_methods);
	lua_pop(L, 1);
	
	luaL_newlib(L, luarure_funcs);
	luarure_push_flag_array(L);
	lua_setfield(L, -2, "flags");
	
	return 1;
}

#undef NEW_METATABLE