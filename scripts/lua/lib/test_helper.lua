-----------------------------------------------------------------------------
-- Imports and dependencies
-----------------------------------------------------------------------------
local math = require('math')
local os = require('os')
local string = require('string')
local table = require('table')
local nscp = require('nscp')

-----------------------------------------------------------------------------
-- Module declaration
-----------------------------------------------------------------------------
module("test_helper", package.seeall)
local valid_chars = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
	"0","1","2","3","4","5","6","7","8","9","-"}
local core = nscp.Core()

math.randomseed(os.time())

function random(len) -- args: smallest and largest possible password lengths, inclusive
	pass = {}
	for z = 1,len do
		case = math.random(1,2)
		a = math.random(1,#valid_chars)
		if case == 1 then
			x=string.upper(valid_chars[a])
		elseif case == 2 then
			x=string.lower(valid_chars[a])
		end
	table.insert(pass, x)
	end
	return(table.concat(pass))
end
string.random = random

function status_to_int(status)
	if status == 'ok' then
		return 0
	elseif status == 'warn' then
		return 1
	elseif status == 'crit' then
		return 2
	elseif status == 'unknown' then
		return 3
	else
		core:log('error', "Unknown status: "..status)
		return 3
	end
end

TestResult = { status = true; children = {} }
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
	table.insert(self.children,result)
end
function TestResult:add_message(result, message)
	table.insert(self.children,TestResult:new{status=result, message=message})
end
function TestResult:assert_equals(a, b, message)
	if a==b then
		table.insert(self.children,TestResult:new{status=true, message=message})
	else
		table.insert(self.children,TestResult:new{status=false, message=message..': '..tostring(a)..' != '..tostring(b)})
	end
end


function TestResult:print(indent)
	indent = indent or 0
	local pad = string.rep(' ', indent)
	if self.status then
		core:log("info", pad .. "[OK ] - " .. self.message)
	else
		core:log("error", pad .. "[ERR] - " .. self.message)
	end
	if # self.children > 0 then
		for i,v in ipairs(self.children) do v:print(indent+2) end
	end
end

function TestResult:print_failed(indent)
	indent = indent or 0
	local pad = string.rep(' ', indent)
	if not self.status then
		core:log("error", pad .. "[ERR] - " .. self.message)
	end
	if # self.children > 0 then
		for i,v in ipairs(self.children) do v:print_failed(indent+2) end
	end
end

function TestResult:count()
	local ok = 0
	local err = 0
	if self.status then
		ok = ok + 1
	else
		err = err + 1
	end
	if # self.children > 0 then
		for i,v in ipairs(self.children) do
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
		return 'crit', tostring(err)..' test cases failed', ''
	else
		return 'ok', tostring(ok)..' test cases succeded', ''
	end
end

local test_cases = {}
function install_test_manager(cases)
	test_cases = cases
	for i=1,# test_cases do
		test_cases[i]:install({})
	end
	return 'ok'
end

local test_cases = {}
function init_test_manager(cases)
	test_cases = cases
	local reg = nscp.Registry()
	reg:simple_query('lua_unittest', lua_unittest_handler, 'TODO')
end

function lua_unittest_handler(command, args)
	local result = TestResult:new{message='Running testsuite'}
	for i=1,# test_cases do
		local case_result = TestResult:new{message='Running testsuite'}
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
