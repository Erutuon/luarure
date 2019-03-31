#! /usr/bin/env lua
local rure = require "luarure"

local re = rure.new("\\p{Latn}")

assert(re:find "a" == "a")

local re = rure.new("(\\p{Uppercase})(\\p{Lowercase}+)")
local captures = re:find_captures("Test")
assert(captures[1] == "T" and captures[2] == "est" and captures[3] == nil)