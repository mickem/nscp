-- Protobuf-free Lua port of scripts/python/test_check_helpers.py.
--
-- Integration tests for the CheckHelpers module: every command it exposes
-- (check_ok/warning/critical, check_version, check_always_*, check_multi,
-- check_negate, check_timeout) plus the alias subsystem under
-- [/settings/<instance>/alias] - all driven through core:simple_query and
-- asserted on status + message. Deliberately does NOT load CheckExternalScripts:
-- this proves aliases-without-external-scripts works end to end. No protobuf, no
-- network - a good CheckHelpers target under the sanitizer build.
--
-- A fuller companion to test_checks.lua (which is a minimal smoke test).
local test = require("test_helper")

local INSTANCE = 'test_check_helpers'

local CheckHelpersTest = {}

function CheckHelpersTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', INSTANCE, 'CheckHelpers')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_check_helpers', 'test_check_helpers.lua')

	-- Alias definitions (prefixed tch_ = Test Check Helpers) under this
	-- instance's alias section, mirroring the Python test.
	local a = '/settings/' .. INSTANCE .. '/alias'
	conf:set_string(a, 'tch_alias_ok', 'check_ok message="fixed via alias"')
	conf:set_string(a, 'tch_alias_warn', 'check_warning message="warn via alias"')
	conf:set_string(a, 'tch_alias_msg', 'check_ok "message=$ARG1$"')
	conf:set_string(a, 'tch_alias_msg_pct', 'check_ok "message=%ARG1%"')
end

function CheckHelpersTest:setup() end
function CheckHelpersTest:teardown() end

-- Run a command and assert its status, and optionally that the message contains
-- a given substring.
function CheckHelpersTest:expect(suite, command, args, expected_status, contains, label)
	label = label or command
	local core = Core()
	local ret, msg = core:simple_query(command, args)
	suite:assert_equals(ret, expected_status, label .. ': status')
	if contains ~= nil then
		suite:assert_contains(msg, contains, string.format('%s: message contains "%s"', label, contains))
	end
	return ret, msg
end

function CheckHelpersTest:test_simple_status(ret)
	local suite = test.TestResult:new{message = 'Simple status commands'}
	self:expect(suite, 'check_ok', {}, 'ok', nil, 'check_ok (default)')
	self:expect(suite, 'check_ok', {'message=hello'}, 'ok', 'hello', 'check_ok (message)')
	self:expect(suite, 'check_warning', {}, 'warning', nil, 'check_warning (default)')
	self:expect(suite, 'check_warning', {'message=careful'}, 'warning', 'careful', 'check_warning (message)')
	self:expect(suite, 'check_critical', {}, 'critical', nil, 'check_critical (default)')
	self:expect(suite, 'check_critical', {'message=on fire'}, 'critical', 'on fire', 'check_critical (message)')
	ret:add(suite)
end

function CheckHelpersTest:test_version(ret)
	local suite = test.TestResult:new{message = 'check_version'}
	local core = Core()
	local rc, msg = core:simple_query('check_version', {})
	suite:assert_equals(rc, 'ok', 'check_version: status OK')
	suite:add_message(msg ~= nil and #msg > 0, 'check_version: returns a non-empty version string')
	ret:add(suite)
end

function CheckHelpersTest:test_always_status(ret)
	local suite = test.TestResult:new{message = 'check_always_* coerce wrapped result'}
	-- check_always_* use POSITIONAL args: first token is the wrapped command.
	self:expect(suite, 'check_always_ok', {'check_critical', 'message=ignored'}, 'ok',
		nil, 'check_always_ok wrapping check_critical -> OK')
	self:expect(suite, 'check_always_warning', {'check_ok', 'message=ignored'}, 'warning',
		nil, 'check_always_warning wrapping check_ok -> WARNING')
	self:expect(suite, 'check_always_critical', {'check_ok', 'message=ignored'}, 'critical',
		nil, 'check_always_critical wrapping check_ok -> CRITICAL')
	ret:add(suite)
end

function CheckHelpersTest:test_multi(ret)
	local suite = test.TestResult:new{message = 'check_multi takes worst result'}
	self:expect(suite, 'check_multi', {'command=check_ok', 'command=check_warning'}, 'warning',
		nil, 'check_multi(OK, WARN) -> WARN')
	self:expect(suite, 'check_multi', {'command=check_ok', 'command=check_warning', 'command=check_critical'},
		'critical', nil, 'check_multi(OK, WARN, CRIT) -> CRIT')
	self:expect(suite, 'check_multi', {'command=check_ok'}, 'ok', nil, 'check_multi(OK) -> OK')
	ret:add(suite)
end

function CheckHelpersTest:test_negate(ret)
	local suite = test.TestResult:new{message = 'check_negate'}
	self:expect(suite, 'check_negate', {'ok=critical', 'command=check_ok'}, 'critical',
		nil, 'check_negate(ok->critical) on check_ok')
	self:expect(suite, 'check_negate', {'critical=ok', 'command=check_critical'}, 'ok',
		nil, 'check_negate(critical->ok) on check_critical')
	ret:add(suite)
end

function CheckHelpersTest:test_timeout(ret)
	local suite = test.TestResult:new{message = 'check_timeout'}
	self:expect(suite, 'check_timeout', {'timeout=10', 'command=check_ok', 'arguments=message=fast'}, 'ok',
		'fast', 'check_timeout(10s, check_ok)')
	ret:add(suite)
end

function CheckHelpersTest:test_aliases(ret)
	local suite = test.TestResult:new{message = 'CheckHelpers aliases'}
	self:expect(suite, 'tch_alias_ok', {}, 'ok', 'fixed via alias',
		'alias dispatches to check_ok with fixed message')
	self:expect(suite, 'tch_alias_warn', {}, 'warning', 'warn via alias',
		'alias dispatches to check_warning')
	self:expect(suite, 'tch_alias_msg', {'hello via $arg'}, 'ok', 'hello via $arg',
		'$ARG1$ substituted into pre-declared message=')
	self:expect(suite, 'tch_alias_msg_pct', {'hello via pct'}, 'ok', 'hello via pct',
		'%ARG1% substituted into pre-declared message=')
	-- Missing $ARG1$: the command still dispatches and the literal placeholder
	-- reaches check_ok, which echoes it back (pins current behaviour).
	self:expect(suite, 'tch_alias_msg', {}, 'ok', '$ARG1$',
		'missing $ARG1$ leaves placeholder verbatim')
	ret:add(suite)
end

function CheckHelpersTest:run()
	local ret = test.TestResult:new{message = 'CheckHelpers test suite'}
	self:test_simple_status(ret)
	self:test_version(ret)
	self:test_always_status(ret)
	self:test_multi(ret)
	self:test_negate(ret)
	self:test_timeout(ret)
	self:test_aliases(ret)
	return ret
end

local instances = { CheckHelpersTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
