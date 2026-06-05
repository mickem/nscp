-----------------------------------------------------------------------------
-- NSClient++ Lua unit-test helper.
--
-- Ported to Lua 5.4 and the current scripting API:
--   * no `module(...)` (removed in 5.2+) - this file returns a module table;
--   * no `require('nscp')` - the runtime exposes `Core()`, `Registry()` and
--     `Settings()` as global constructors and `nscp.sleep/print/log` helpers.
--
-- Public API (via the returned table, conventionally `local test = require("test_helper")`):
--   test.TestResult            - result tree / assertion helpers
--   test.random(len)           - random string (also installed as string.random)
--   test.status_to_int(status) - 'ok'/'warn'/'crit'/'unknown' -> nagios code
--   test.init_test_manager(t)  - register the `lua_unittest` query (call at load)
--   test.install_test_manager(t) - run every case's :install() (call from main)
-----------------------------------------------------------------------------
local math = require('math')
local os = require('os')
local string = require('string')
local table = require('table')

local M = {}

local core = Core()

local valid_chars = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
	"0","1","2","3","4","5","6","7","8","9","-"}

math.randomseed(os.time())

function M.random(len) -- length of the random string to produce
	local pass = {}
	for _ = 1, len do
		local a = math.random(1, #valid_chars)
		if math.random(1, 2) == 1 then
			table.insert(pass, string.upper(valid_chars[a]))
		else
			table.insert(pass, string.lower(valid_chars[a]))
		end
	end
	return table.concat(pass)
end
-- Keep the historical `string.random(len)` convenience used by older test scripts.
string.random = M.random

function M.status_to_int(status)
	if status == 'ok' then
		return 0
	elseif status == 'warn' then
		return 1
	elseif status == 'crit' then
		return 2
	elseif status == 'unknown' then
		return 3
	else
		core:log('error', "Unknown status: " .. tostring(status))
		return 3
	end
end

local TestResult = { status = true, children = {} }
M.TestResult = TestResult
function TestResult:new(o)
	o = o or {}
	o["children"] = o["children"] or {}
	if o["status"] == nil then o["status"] = true end
	setmetatable(o, self)
	self.__index = self
	return o
end
function TestResult:add(result)
	if not result then
		error("invalid result")
	end
	if not result.status then self.status = false end
	table.insert(self.children, result)
end
function TestResult:add_message(result, message)
	if not result then self.status = false end
	table.insert(self.children, TestResult:new{status = result, message = message})
end
function TestResult:assert_equals(a, b, message)
	if a == b then
		table.insert(self.children, TestResult:new{status = true, message = message})
	else
		self.status = false
		table.insert(self.children, TestResult:new{status = false, message = message .. ': ' .. tostring(a) .. ' != ' .. tostring(b)})
	end
end
-- Substring assertions. `needle` is matched literally (plain find, no Lua patterns).
function TestResult:assert_contains(haystack, needle, message)
	if type(haystack) == 'string' and string.find(haystack, needle, 1, true) then
		table.insert(self.children, TestResult:new{status = true, message = message})
	else
		self.status = false
		table.insert(self.children, TestResult:new{status = false,
			message = message .. ': "' .. tostring(needle) .. '" not in "' .. tostring(haystack) .. '"'})
	end
end
function TestResult:assert_not_contains(haystack, needle, message)
	if type(haystack) == 'string' and string.find(haystack, needle, 1, true) then
		self.status = false
		table.insert(self.children, TestResult:new{status = false,
			message = message .. ': unexpected "' .. tostring(needle) .. '" in "' .. tostring(haystack) .. '"'})
	else
		table.insert(self.children, TestResult:new{status = true, message = message})
	end
end

function TestResult:print(indent)
	indent = indent or 0
	local pad = string.rep(' ', indent)
	if self.status then
		core:log("info", pad .. "[OK ] - " .. tostring(self.message))
	else
		core:log("error", pad .. "[ERR] - " .. tostring(self.message))
	end
	if #self.children > 0 then
		for _, v in ipairs(self.children) do v:print(indent + 2) end
	end
end

function TestResult:print_failed(indent)
	indent = indent or 0
	local pad = string.rep(' ', indent)
	if not self.status then
		core:log("error", pad .. "[ERR] - " .. tostring(self.message))
	end
	if #self.children > 0 then
		for _, v in ipairs(self.children) do v:print_failed(indent + 2) end
	end
end

function TestResult:count()
	local ok = 0
	local err = 0
	if self.status then ok = ok + 1 else err = err + 1 end
	if #self.children > 0 then
		for _, v in ipairs(self.children) do
			local lok, lerr = v:count()
			ok = ok + lok
			err = err + lerr
		end
	end
	return ok, err
end

function TestResult:get_nagios()
	local ok, err = self:count()
	if not self.status then
		return 'crit', tostring(err) .. ' test cases failed', ''
	else
		return 'ok', tostring(ok) .. ' test cases succeeded', ''
	end
end

local test_cases = {}

function M.install_test_manager(cases)
	test_cases = cases
	for i = 1, #test_cases do
		test_cases[i]:install({})
	end
	return 'ok', 'installed'
end

local function lua_unittest_handler(command, args)
	local result = TestResult:new{message = 'Running testsuite'}
	for i = 1, #test_cases do
		local case_result = TestResult:new{message = 'Running testsuite'}
		test_cases[i]:setup()
		case_result:add(test_cases[i]:run())
		test_cases[i]:teardown()
		result:add(case_result)
	end
	result:print()
	core:log("info", "--//Failed tests//---")
	result:print_failed()
	return result:get_nagios()
end

function M.init_test_manager(cases)
	test_cases = cases
	local reg = Registry()
	reg:simple_query('lua_unittest', lua_unittest_handler, 'Run the lua unit test suite')
end

return M
