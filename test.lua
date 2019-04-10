#! /usr/bin/env lua
local rure = require "luarure"

local re = rure.new "\\p{Latn}"

assert(re:find "a" == "a")

local NIL = {}
local function check_captures(captures, fields)
	assert(captures ~= nil)
	
	for k, v in pairs(fields) do
		if v == NIL then
			v = nil
		end
		
		assert(captures[k] == v,
			"field " .. tostring(k) .. " equaled " .. tostring(captures[k])
			.. ", not " .. tostring(v))
	end
	
	local len = #fields
	assert(#captures == len,
		"length of captures is " .. tostring(#captures)
		.. ", not " .. tostring(len))
	assert(captures[len + 1] == nil)
	
	assert(captures.nonexistent == nil)
	
	if type(captures) ~= "table" then
		local captures_table = captures:to_table()
		check_captures(captures_table, fields)
	end
end

local re = rure.new "(\\p{Uppercase})(\\p{Lowercase}+)"
local captures = re:find_captures "Test"
check_captures(captures, {
	[0] = "Test",
	[1] = "T",
	[2] = "est",
})

local re = rure.new "(?P<capital>\\p{Upper})(\\p{Lower})(?P<lowercase>\\p{Lower}+)"
local captures = re:find_captures "Test"
check_captures(captures, {
	[0] = "Test",
	[1] = "T",
	[2] = "e",
	[3] = "st",
	capital = "T",
	lowercase = "st",
})

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
	check_captures(captures, expected_captures[i])
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
check_captures(captures, {
	[0] = "Capitalized",
	[1] = "C",
	[2] = "apitalized",
	capital = "C",
	lower = "apitalized",
})

for i, flag in ipairs {
	[0] = "CASEI", "MULTI", "DOTNL", "SWAP_GREED", "SPACE", "UNICODE"
} do
	assert(rure.flags[i] == flag)
end