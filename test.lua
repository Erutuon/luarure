#! /usr/bin/env lua
local rure = require "luarure"

local re = rure.new "\\p{Latn}"

assert(re:find "a" == "a")

local re = rure.new "(\\p{Uppercase})(\\p{Lowercase}+)"
local captures = re:find_captures "Test"
assert(captures[0] == "Test")
assert(captures[1] == "T")
assert(captures[2] == "est")
assert(captures[3] == nil)
assert(captures.nonexistent == nil)

local re = rure.new "(?P<capital>\\p{Upper})(\\p{Lower})(?P<lowercase>\\p{Lower}+)"
local captures = re:find_captures "Test"
assert(#captures == 4) -- 0, 1, 2, 3; length of equivalent Lua array would be 3
assert(captures[0] == "Test")
assert(captures[1] == "T")
assert(captures[2] == "e")
assert(captures[3] == "st")
assert(captures[4] == nil)
assert(captures.capital == "T")
assert(captures.lowercase == "st")
assert(captures.nonexistent == nil)

local re = rure.new "\\p{Latn}"
local str = "abc"
local iter = str:gmatch "."
for match in re:iter(str) do
	assert(match == iter())
end

local re = rure.new "(?P<capital>\\p{Upper})(?P<lowercase>\\p{Lower}*)"
local str = "Hello, World!"
local expected_captures = {
	{ [0] = "Hello", "H", "ello", capital = "H", lowercase = "ello" },
	{ [0] = "World", "W", "orld", capital = "W", lowercase = "orld" },
}
local i = 0
for captures in re:iter_captures(str) do
	i = i + 1
	local expected = expected_captures[i]
	assert(#captures == #expected + 1)
	for k, v in pairs(expected) do
		assert(captures[k] == v)
	end
	assert(captures.nonexistent == nil)
end

-- The __gc metamethod should prevent a userdatum from being used by a method.
local gc_func = getmetatable(re) and getmetatable(re).__gc
assert(gc_func and pcall(function () gc_func(re):is_match 'a' end) == false)