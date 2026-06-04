-- Protobuf-free Lua port of scripts/python/test_external_script.py (Linux paths).
--
-- Drives the CheckExternalScripts module over the core via simple_query(): it
-- configures a set of shell-script commands (scripts/check_ok.sh and
-- scripts/check_test.sh), then runs them with and without arguments and asserts
-- the returned state + message. This exercises the external-script argv-isolation
-- and "allow arguments"/"allow nasty characters" gating - a good CheckExternalScripts
-- target under the sanitizer build. No protobuf, no network.
--
-- Replaces the previous test_ext_script.lua, which was a stale copy of the old
-- protobuf-based NRPE test and did not exercise external scripts at all.
local test = require("test_helper")

-- check_test.sh, when invoked with $1 == "LONG", prints the header line followed
-- by 11 identical digit lines. tes_sca_sh maps user arg 1 -> $ARG1$ and leaves
-- $ARG2$/$ARG3$ unsubstituted (argv-isolation, 0.13+), so the header reads
-- "(LONG $ARG2$ $ARG3$)".
local DIGITS = '0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789'
local LONG_OUTPUT = 'Test arguments are: (LONG $ARG2$ $ARG3$)'
for _ = 1, 11 do LONG_OUTPUT = LONG_OUTPUT .. '\n' .. DIGITS end

local ExtScriptTest = {}

-- Normalise a message the same way the Python test does so transport/templating
-- artefacts do not cause spurious diffs:
--   * when `cleanup`, drop the double quotes the operator template tokeniser
--     consumes before argv is built (so they never reach the script);
--   * collapse CR/LF and doubled newlines.
local function normalise(s, cleanup)
	if cleanup then s = s:gsub('"', '') end
	s = s:gsub('\r', '\n')
	s = s:gsub('\n\n', '\n')
	return s
end

function ExtScriptTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', 'test_external_scripts', 'CheckExternalScripts')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_ext_script', 'test_ext_script.lua')

	-- Start locked down; run() flips these on and reloads as it progresses.
	conf:set_string('/settings/test_external_scripts', 'allow arguments', 'false')
	conf:set_string('/settings/test_external_scripts', 'allow nasty characters', 'false')

	local s = '/settings/test_external_scripts/scripts'
	conf:set_string(s, 'tes_UPPER_lower', 'scripts/check_ok.sh')
	conf:set_string(s, 'tes_script_ok', 'scripts/check_ok.sh')
	conf:set_string(s, 'tes_script_sh', 'scripts/check_test.sh')
	conf:set_string(s, 'tes_script_test', 'scripts/check_test.sh')
	conf:set_string(s, 'tes_sa_test', 'scripts/check_test.sh "ARG1" "ARG 2" "A R G 3"')
	conf:set_string(s, 'tes_sca_sh', 'scripts/check_test.sh $ARG1$ "$ARG2$" "$ARG3$"')

	local a = '/settings/test_external_scripts/alias'
	conf:set_string(a, 'tes_alias_ok', 'tes_script_test')
	conf:set_string(a, 'tes_aa_ok', 'tes_script_test  "ARG1" "ARG 2" "A R G 3"')
	conf:set_string(a, 'alias_UPPER_lower', 'tes_UPPER_lower')
end

function ExtScriptTest:setup() end
function ExtScriptTest:teardown() end

-- Run one script and assert its (state, message). `cleanup` defaults to true.
function ExtScriptTest:do_one_test(script, expected, message, args, cleanup)
	if cleanup == nil then cleanup = true end
	expected = expected or 'ok'
	args = args or {}
	local result = test.TestResult:new{message = string.format('%s (%s)', script, table.concat(args, ', '))}

	local core = Core()
	local ret, msg = core:simple_query(script, args)

	message = normalise(message, cleanup)
	msg = normalise(msg, false)

	result:assert_equals(ret, expected, 'Validate return code for ' .. script)
	result:assert_equals(msg, message, 'Validate return message for ' .. script)
	return result
end

function ExtScriptTest:run()
	local conf = Settings()
	local core = Core()
	local ret = test.TestResult:new{message = 'External scripts test suite'}

	-- Arguments NOT allowed.
	local result = test.TestResult:new{message = 'Arguments NOT allowed'}
	result:add(self:do_one_test('tes_script_ok', 'ok', 'OK: Everything is going to be fine'))
	result:add(self:do_one_test('tes_script_test', 'ok', 'Test arguments are: (  )'))
	result:add(self:do_one_test('tes_sa_test', 'ok', 'Test arguments are: ("ARG1" "ARG 2" "A R G 3")'))
	result:add(self:do_one_test('tes_script_test', 'unknown', 'Arguments not allowed see nsclient.log for details', {'NOT ALLOWED'}))
	ret:add(result)

	conf:set_string('/settings/test_external_scripts', 'allow arguments', 'true')
	core:reload('test_external_scripts')

	-- Arguments allowed.
	result = test.TestResult:new{message = 'Arguments allowed'}
	local sub = test.TestResult:new{message = 'sh'}
	sub:add(self:do_one_test('tes_sca_sh', 'ok', 'Test arguments are: (OK "$ARG2$" "$ARG3$")', {'OK'}))
	sub:add(self:do_one_test('tes_sca_sh', 'warning', 'Test arguments are: (WARN "$ARG2$" "$ARG3$")', {'WARN'}))
	sub:add(self:do_one_test('tes_sca_sh', 'critical', 'Test arguments are: (CRIT "$ARG2$" "$ARG3$")', {'CRIT'}))
	sub:add(self:do_one_test('tes_sca_sh', 'unknown', 'Test arguments are: (UNKNOWN "$ARG2$" "$ARG3$")', {'UNKNOWN'}))
	sub:add(self:do_one_test('tes_sca_sh', 'ok', 'Test arguments are: (OK "String with space" "A long long option with many spaces")',
		{'OK', 'String with space', 'A long long option with many spaces'}))
	sub:add(self:do_one_test('tes_sca_sh', 'ok', LONG_OUTPUT, {'LONG'}))
	sub:add(self:do_one_test('tes_sca_sh', 'unknown',
		'Request contained illegal characters set /settings/external scripts/allow nasty characters=true!',
		{'OK', '$$$ \\ \\', '$$$ \\ \\'}))
	result:add(sub)

	sub = test.TestResult:new{message = 'Upper and lower case'}
	sub:add(self:do_one_test('tes_upper_LOWER', 'ok', 'OK: Everything is going to be fine', {'OK'}))
	sub:add(self:do_one_test('alias_UPPER_lower', 'ok', 'OK: Everything is going to be fine', {'OK'}))
	result:add(sub)
	ret:add(result)

	conf:set_string('/settings/test_external_scripts', 'allow nasty characters', 'true')
	core:reload('test_external_scripts')

	-- Nasty arguments allowed: each "$ \ \" is a single argv element under
	-- argv-isolation, printed verbatim with no shell mangling.
	result = test.TestResult:new{message = 'Nasty Arguments allowed'}
	sub = test.TestResult:new{message = 'sh'}
	sub:add(self:do_one_test('tes_sca_sh', 'ok', 'Test arguments are: (OK $ \\ \\ $ \\ \\)',
		{'OK', '$ \\ \\', '$ \\ \\'}, false))
	result:add(sub)
	ret:add(result)

	return ret
end

local instances = { ExtScriptTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
