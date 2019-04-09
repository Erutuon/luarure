#! /usr/bin/env lua
local rure = require "luarure"

local re = rure.new "\\p{Latn}"

assert(re:find "a" == "a")

local re = rure.new "(\\p{Uppercase})(\\p{Lowercase}+)"
local captures = re:find_captures "Test"
assert(captures ~= nil)
assert(#captures == 3) -- 0, 1, 2; length of equivalent Lua array would be 2
assert(captures[0] == "Test")
assert(captures[1] == "T")
assert(captures[2] == "est")
assert(captures[3] == nil)
assert(captures.nonexistent == nil)

local re = rure.new "(?P<capital>\\p{Upper})(\\p{Lower})(?P<lowercase>\\p{Lower}+)"
local captures = re:find_captures "Test"
assert(captures ~= nil)
assert(#captures == 4)
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
assert(i == 2)

-- The __gc metamethod should prevent a userdatum from being used by a method.
local gc_func = getmetatable(re) and getmetatable(re).__gc
assert(gc_func and pcall(function () gc_func(re):is_match 'a' end) == false)

local re
assert(pcall(function () re = rure.new("^line \\d+$", "multi") end) == true)
assert(re:find "line one\nline 2\nline 3" == "line 2")

assert(pcall(function () re = rure.new("αβγδ", "casei", "unicode") end) == true)
assert(re:is_match "ΑΒΓΔ")

assert(pcall(function () re = rure.new("ΑΒΓΔ", "CASEI", "UNICODE") end) == true)
assert(re:is_match "αβγδ")

assert(pcall(function () re = rure.new("a.b", "dotnl") end) == true)
assert(re:is_match "a\nb")

assert(pcall(function () re = rure.new("a.+?b", "swap_greed") end) == true)
assert(re:find "abab" == "abab")


assert(pcall(function ()
	re = rure.new([[
		^
		\b
		(?P<capital>\p{Uppercase}) # initial capital letter
		(?P<lower>\p{Lowercase}+)  # following lowercase letters
		\b
		$
	]], "space", "unicode")
end) == true)

local captures = re:find_captures "Capitalized"
assert(captures ~= nil)
assert(#captures == 3)
assert(captures[0] == "Capitalized")
assert(captures[1] == "C")
assert(captures[2] == "apitalized")
assert(captures[3] == nil)
assert(captures.capital == "C")
assert(captures.lower == "apitalized")
assert(captures.nonexistent == nil)

for i, flag in ipairs {
	[0] = "CASEI", "MULTI", "DOTNL", "SWAP_GREED", "SPACE", "UNICODE"
} do
	assert(rure.flags[i] == flag)
end