-- Protobuf-free Lua port of scripts/python/test_log_file.py.
--
-- Drives the CheckLogFile module's `check_logfile` over the core via
-- simple_query(): it writes a small CSV fixture to a temp file, then runs a
-- battery of filter-operator and warn/crit-boundary checks and asserts the
-- matched row count (from perfdata) and the resulting Nagios state. No protobuf
-- and no network - just the command-dispatch + filter-engine path, which makes
-- it a useful CheckLogFile target under the sanitizer build.
local test = require("test_helper")

local LogFileTest = {}

-- The fixture: 6 newline-terminated CSV rows (column1,column2,column3).
local FIXTURE = {
	"1,A,Test 1",
	"2,A,Test 2",
	"3,B,Test 1",
	"4,B,Test 1",
	"5,C,Test 1",
	"6,B,Test 2",
}

function LogFileTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', 'test_disk', 'CheckLogFile')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_log_file', 'test_log_file.lua')
end

function LogFileTest:setup()
	-- A unique temp file per run so concurrent/leftover runs never collide.
	self.work_file = os.tmpname()
	local f = assert(io.open(self.work_file, 'w'))
	for _, line in ipairs(FIXTURE) do
		f:write(line .. '\n')
	end
	f:close()
end

function LogFileTest:teardown()
	if self.work_file then os.remove(self.work_file) end
end

-- Pull the integer count out of a "'label'=N;warn;crit" performance-data string.
local function get_count(perf)
	if not perf or perf == '' then return -1 end
	local n = perf:match('=(%d+)')
	if not n then return -1 end
	return tonumber(n)
end

-- Count matched rows for a filter and assert it equals `expected` (with the
-- warn/crit thresholds set to expected so a faithful engine returns OK).
function LogFileTest:check_files(filter, text, expected)
	local core = Core()
	local result = test.TestResult:new{message = string.format('Checking %s: %s', text, filter)}
	local args = {
		'file=' .. self.work_file,
		'column-split=,',
		'filter=' .. filter,
		'warn=count gt ' .. tostring(expected),
		'crit=count gt ' .. tostring(expected),
	}
	local ret, msg, perf = core:simple_query('check_logfile', args)
	local count = get_count(perf)
	result:add_message(count == expected,
		string.format('%s - row count (got %s expected %d)', filter, tostring(count), expected))
	result:add_message(ret == 'ok',
		string.format('%s - status (got %s expected ok)', filter, tostring(ret)))
	return result
end

-- Run a filter with explicit warn/crit expressions and assert the Nagios state.
function LogFileTest:check_bound(filter, warn, crit, expected)
	local core = Core()
	local result = test.TestResult:new{message = string.format('Checking %s/%s/%s', filter, warn, crit)}
	local args = {
		'file=' .. self.work_file,
		'column-split=,',
		'filter=' .. filter,
		'warn=' .. warn,
		'crit=' .. crit,
	}
	local ret, msg, perf = core:simple_query('check_logfile', args)
	result:assert_equals(ret, expected,
		string.format('Check status for %s/%s/%s', filter, warn, crit))
	return result
end

function LogFileTest:run_filter_operator_test()
	local result = test.TestResult:new{message = 'Filter tests'}
	result:add(self:check_files('none', 'Count all lines', 6))
	result:add(self:check_files("column2 = 'A'", 'Count all A', 2))
	result:add(self:check_files("column2 = 'B'", 'Count all B', 3))
	result:add(self:check_files("column2 = 'C'", 'Count all C', 1))
	result:add(self:check_files("column3 = 'Test 1'", 'Count all T1', 4))
	result:add(self:check_files("column3 like 'Test'", 'Count all T', 6))
	result:add(self:check_files("column3 not like '1'", 'Count not T1', 2))
	result:add(self:check_files("column1 > 1", 'Count >1', 5))
	result:add(self:check_files("column1 > 3", 'Count >3', 3))
	result:add(self:check_files("column1 > 5", 'Count >5', 1))
	result:add(self:check_files("column1 < 1", 'Count <1', 0))
	result:add(self:check_files("column1 < 3", 'Count <3', 2))
	result:add(self:check_files("column1 < 5", 'Count <5', 4))
	result:add(self:check_files("column1 = 1", 'Count =1', 1))
	result:add(self:check_files("column1 = 3", 'Count =3', 1))
	result:add(self:check_files("column1 = 5", 'Count =5', 1))
	result:add(self:check_files("column1 != 1", 'Count !=1', 5))
	result:add(self:check_files("column1 != 3", 'Count !=3', 5))
	result:add(self:check_files("column1 != 5", 'Count !=5', 5))
	return result
end

function LogFileTest:run_boundry_test()
	local result = test.TestResult:new{message = 'Boundry tests'}
	result:add(self:check_bound('none', 'count > 1', 'none', 'warning'))
	result:add(self:check_bound('none', 'none', 'count > 1', 'critical'))
	result:add(self:check_bound('column1 > 5', 'count > 2', 'count > 5', 'ok'))
	result:add(self:check_bound('column1 > 4', 'count > 2', 'count > 5', 'ok'))
	result:add(self:check_bound('column1 > 3', 'count > 2', 'count > 5', 'warning'))
	result:add(self:check_bound('column1 > 2', 'count > 2', 'count > 5', 'warning'))
	result:add(self:check_bound('column1 > 1', 'count > 2', 'count > 5', 'warning'))
	result:add(self:check_bound('column1 > 0', 'count > 2', 'count > 5', 'critical'))
	result:add(self:check_bound('column1 > 5', 'column1 = 3', 'none', 'ok'))
	result:add(self:check_bound('column1 > 0', 'column1 = 3', 'none', 'warning'))
	return result
end

function LogFileTest:run()
	local result = test.TestResult:new{message = 'CheckLogFile via simple_query'}
	result:add(self:run_filter_operator_test())
	result:add(self:run_boundry_test())
	return result
end

local instances = { LogFileTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
