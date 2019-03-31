#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <rure.h> // stdbool.h included here

#define RURE_NAME "rure"
#define RURE_CAPTURES_NAME "rure_captures"
#define CHECK_RURE(L, i) (* (rure * *) luaL_checkudata((L), (i), RURE_NAME))

static inline rure * lua_checkrure(lua_State * L, int i) {
	return * (rure * *) luaL_checkudata(L, i, RURE_NAME);
}

static inline rure_captures * lua_checkrurecaptures (lua_State * L, int i) {
	return * (rure_captures * *) luaL_checkudata(L, i, RURE_CAPTURES_NAME);
}

// rure captures metamethods
static int luarure_captures_gc (lua_State * L) {
	rure_captures * captures = lua_checkrurecaptures(L, 1);
	rure_captures_free(captures);
	return 0;
}

// rure captures methods
static int luarure_captures_index (lua_State * L) {
	rure_captures * captures = lua_checkrurecaptures(L, 1);
	switch (lua_type(L, 2)) {
		case LUA_TNUMBER: {
			if (lua_isinteger(L, 2)) {
				lua_Integer index = lua_tointeger(L, 2);
				rure_match match;
				
				if (index >= 0 && rure_captures_at(captures, (size_t) index, &match)
				&& lua_getuservalue(L, 1) == LUA_TTABLE
				&& lua_getfield(L, -1, "haystack") == LUA_TSTRING) {
					const char * str = lua_tostring(L, -1);
					lua_pushlstring(L, &str[match.start], match.end - match.start);
					return 1;
				}
			}
			break;
		}
		case LUA_TSTRING: {
			if (lua_getuservalue(L, 1) == LUA_TTABLE
			&& lua_getfield(L, -1, "regex") == LUA_TUSERDATA) {
				rure * regex = lua_checkrure(L, -1);
				const char * name = lua_tostring(L, 2);
				int32_t index = rure_capture_name_index(regex, name);
				rure_match match;
				
				if (index != -1
				&& rure_captures_at(captures, (size_t) index, &match)
				&& lua_getfield(L, -2, "haystack") == LUA_TSTRING) {
					const char * str = lua_tostring(L, -1);
					lua_pushlstring(L, &str[match.start], match.end - match.start);
					return 1;
				}
			}
			break;
		}
		default: (void) (0);
	}
	
	lua_pushnil(L);
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
	
	rure * * ud = lua_newuserdata(L, sizeof (rure *));
	*ud = regex;
	luaL_setmetatable(L, RURE_NAME);
	
	return 1;
}

// Signal somehow that the regex is dead?
static int luarure_gc (lua_State * L) {
	rure * regex = lua_checkrure(L, 1);
	rure_free(regex);
	return 0;
}

// rure methods
static int luarure_is_match (lua_State * L) {
	rure * regex = lua_checkrure(L, 1);
	size_t len;
	const char * str = luaL_checklstring(L, 2, &len);
	size_t start = luaL_optinteger(L, 3, 1) - 1; // adjust index
	bool res = rure_is_match(regex, (const uint8_t *) str, len, start);
	lua_pushboolean(L, res);
	return 1;
}

static int luarure_find (lua_State * L) {
	rure * regex = lua_checkrure(L, 1);
	size_t len;
	const char * str = luaL_checklstring(L, 2, &len);
	size_t start = luaL_optinteger(L, 3, 1) - 1; // adjust index
	rure_match match;
	bool res = rure_find(regex, (const uint8_t *) str, len, start, &match);
	if (res)
		lua_pushlstring(L, &str[match.start], match.end - match.start);
	else
		lua_pushnil(L);
	return 1;
}

static int luarure_find_captures (lua_State * L) {
	rure * regex = lua_checkrure(L, 1);
	size_t len;
	const char * str = luaL_checklstring(L, 2, &len);
	size_t start = luaL_optinteger(L, 3, 1) - 1; // adjust index
	rure_captures * captures = rure_captures_new(regex);
	bool res = rure_find_captures(regex, (const uint8_t *) str, len, start, captures);
	if (res) {
		rure_captures * * ud = lua_newuserdata(L, sizeof (rure_captures *));
		*ud = captures;
		lua_createtable(L, 0, 2);
#define PUSH_SET(L, t, k, v) (lua_pushvalue((L), (v)), lua_setfield((L), (t) - 1, (k)))
		PUSH_SET(L, -1, "regex", 1);
		PUSH_SET(L, -1, "haystack", 2);
#undef PUSH_SET
		lua_setuservalue(L, -2); // Set as uservalue of captures.
		luaL_setmetatable(L, RURE_CAPTURES_NAME);
	} else
		lua_pushnil(L);
	return 1;
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
	{ NULL, NULL }
};

#undef ENTRY

static const luaL_Reg luarure_captures_metamethods[] = {
	{ "__gc", luarure_captures_gc },
	{ "__index", luarure_captures_index },
	{ NULL, NULL }
};

int luaopen_luarure (lua_State * L) {
	luaL_newmetatable(L, RURE_NAME);
	luaL_setfuncs(L, luarure_metamethods, 0);
	luaL_newlib(L, luarure_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	
	luaL_newmetatable(L, RURE_CAPTURES_NAME);
	luaL_setfuncs(L, luarure_captures_metamethods, 0);
	lua_pop(L, 1);
	
	luaL_newlib(L, luarure_funcs);
	
	return 1;
}

#undef NEW_METATABLE