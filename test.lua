#! /usr/bin/env lua53
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
local next_char = str:gmatch "."
for match in re:iter(str) do
	assert(match == next_char())
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

-- Test metatable-conditioned behavior of rure, rure_captures, and rure_iter
-- objects.

local function test_metatable(obj, method_to_test, ...)
	local mt = getmetatable(obj)
	assert(type(mt) == "table")
	
	local name = mt.__name
	assert(type(name) == "string")
	
	local gc_func = mt.__gc
	assert(type(gc_func) == "function")
	
	gc_func(obj)
	
	local method
	assert(pcall(function ()
			method = obj[method_to_test]
		end) == true,
		"A freed " .. name
		.. " object should be indexable and have the method "
		.. method_to_test .. ".")
	
	assert(type(method) == "function",
		"The method " .. method_to_test .. " is "
		.. (type(method) == "nil" and "nil" or "a " .. type(method))
		.. ", not a function.")
	
	local vararg = { n = select("#", ...), ... }
	assert(pcall(function ()
			method(obj, table.unpack(vararg, 1, vararg.n))
		end) == false,
		"The __gc metamethod should prevent a freed " .. name
		.. " object from being used by a method.")
end

-- rure
test_metatable(rure.new "empty", "is_match", "empty")

-- rure_captures
test_metatable(rure.new "empty":find_captures "empty", "to_table")

-- rure_iter
test_metatable(select(2, rure.new "empty":iter "empty"), "next")

-- Test flags. They should be case-insensitive and the behavior of the
-- resulting regex should be correct.

for i, flag in ipairs {
	[0] = "CASEI", "MULTI", "DOTNL", "SWAP_GREED", "SPACE", "UNICODE"
} do
	assert(rure.flags[i] == flag)
end

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