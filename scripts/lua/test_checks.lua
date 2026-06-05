-- Protobuf-free Lua acceptance test that drives real check commands through the
-- core via simple_query(). It exercises the command-dispatch path and the
-- CheckHelpers module's C++ code, which makes it a useful target for running
-- under the sanitizer build (ASan/LSan) to catch leaks and memory errors.
local test = require("test_helper")

local CheckHelpersTest = {}

function CheckHelpersTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', 'test_checkhelpers', 'CheckHelpers')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_checks', 'test_checks.lua')
end

function CheckHelpersTest:setup() end
function CheckHelpersTest:teardown() end

function CheckHelpersTest:run()
	local core = Core()
	local result = test.TestResult:new{message = 'CheckHelpers via simple_query'}

	local code, msg = core:simple_query('check_ok', {})
	result:assert_equals(code, 'ok', 'check_ok returns OK')

	code, msg = core:simple_query('check_critical', {})
	result:assert_equals(code, 'critical', 'check_critical returns CRITICAL')

	code, msg = core:simple_query('check_warning', {})
	result:assert_equals(code, 'warning', 'check_warning returns WARNING')

	-- check_ok ships a default message; make sure we actually got one back.
	code, msg = core:simple_query('check_ok', {})
	result:add_message(msg ~= nil and msg ~= '', 'check_ok returns a non-empty message')

	return result
end

local instances = { CheckHelpersTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
