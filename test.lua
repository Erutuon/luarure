#! /usr/bin/env lua
local rure = require "luarure"

local re = rure.new "\\p{Latn}"

assert(re:find "a" == "a")

local re = rure.new "(\\p{Uppercase})(\\p{Lowercase}+)"
local captures = re:find_captures "Test"
assert(captures[1] == "T")
assert(captures[2] == "est")
assert(captures[3] == nil)

local re = rure.new "(?P<capital>\\p{Upper})(\\p{Lower})(?P<lowercase>\\p{Lower}+)"
local captures = re:find_captures "Test"
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