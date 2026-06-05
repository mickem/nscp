-- Protobuf-free Lua port of scripts/python/test_uptime.py.
--
-- Validates check_uptime under both `local` and `utc` timezones (issues
-- #365/#452/#590). The timezone is cached per-module in loadModuleEx, so we
-- switch it by writing /settings/default/timezone and reloading CheckSystem.
-- For each zone we assert, via core:simple_query:
--   * the default detail-syntax surfaces the configured zone label (${tz}) and
--     mentions boot:/uptime:, and the opposite label never leaks;
--   * --max-unit=d caps ${uptime} granularity (no week suffix) and resolves ${tz};
--   * an invalid --max-unit is rejected with UNKNOWN;
--   * duration thresholds with unit suffixes (30m/2h/1d/2w) parse cleanly.
-- No protobuf, no network - a CheckSystem(Unix) target under the sanitizer build.
--
-- On Linux check_uptime lives in CheckSystemUnix, whose module name is
-- "CheckSystem" (it builds as libCheckSystem.so); we load it under an alias so
-- reloads don't disturb any other CheckSystem instance.
local test = require("test_helper")

local SYSTEM_ALIAS = 'test_uptime_system'
local SYSTEM_MODULE = 'CheckSystem'

local UptimeTest = {}

function UptimeTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', SYSTEM_ALIAS, SYSTEM_MODULE)
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_uptime', 'test_uptime.lua')
	-- Default timezone for the run; individual cases override.
	conf:set_string('/settings/default', 'timezone', 'local')
end

function UptimeTest:setup() end

function UptimeTest:teardown()
	-- Restore the default so we don't poison subsequent tests in the same run.
	self:set_timezone('local')
end

-- Write the configured timezone and reload CheckSystem so its per-module cache
-- (populated in loadModuleEx) is refreshed.
function UptimeTest:set_timezone(tz)
	local conf = Settings()
	conf:set_string('/settings/default', 'timezone', tz)
	Core():reload(SYSTEM_ALIAS)
	nscp.sleep(500)
end

function UptimeTest:query(args)
	return Core():simple_query('check_uptime', args)
end

function UptimeTest:run_uptime_for(tz, label)
	local result = test.TestResult:new{message = 'check_uptime with timezone=' .. tz}
	self:set_timezone(tz)
	local opposite = (label == 'local') and 'UTC' or 'local'

	-- 1. Default detail-syntax surfaces the configured zone label. Override
	--    warn/crit so the result is OK regardless of actual host uptime.
	local rc, msg = self:query({'show-all', 'warn=uptime < 0', 'crit=uptime < 0'})
	result:add_message(rc == 'ok',
		string.format('default check_uptime OK with timezone=%s (rc=%s)', tz, tostring(rc)))
	result:assert_contains(msg, label, 'default detail-syntax contains tz label ' .. label)
	result:assert_contains(msg, 'boot:', 'default output mentions boot:')
	result:assert_contains(msg, 'uptime:', 'default output mentions uptime:')
	result:assert_not_contains(msg, opposite, 'opposite tz label ' .. opposite .. ' does not appear')

	-- 2. --max-unit=d caps ${uptime} granularity (no week suffix) and resolves ${tz}.
	rc, msg = self:query({'show-all', 'max-unit=d', 'warn=uptime < 0', 'crit=uptime < 0',
		'detail-syntax=uptime=${uptime} tz=${tz}'})
	result:add_message(rc == 'ok',
		string.format('max-unit=d resolves under timezone=%s (rc=%s)', tz, tostring(rc)))
	result:assert_contains(msg, 'uptime=', '${uptime} placeholder substituted')
	result:assert_not_contains(msg, 'w ', 'max-unit=d produces no week suffix')
	result:assert_contains(msg, 'tz=' .. label, '${tz} placeholder resolves to ' .. label)

	-- 2b. An invalid max-unit must be rejected with UNKNOWN.
	rc, msg = self:query({'show-all', 'max-unit=foo'})
	result:assert_equals(rc, 'unknown', 'max-unit=foo fails with UNKNOWN')

	-- 3. Duration thresholds with unit suffixes must parse cleanly (#452).
	for _, spec in ipairs({'30m', '2h', '1d', '2w'}) do
		rc, msg = self:query({'show-all', 'warn=uptime < ' .. spec, 'detail-syntax=ok'})
		result:add_message(rc ~= 'unknown',
			string.format('uptime < %s parses cleanly under timezone=%s (rc=%s)', spec, tz, tostring(rc)))
	end

	return result
end

function UptimeTest:run()
	local master = test.TestResult:new{message = 'Testing check_uptime under local and utc'}
	master:add(self:run_uptime_for('local', 'local'))
	master:add(self:run_uptime_for('utc', 'UTC'))
	return master
end

local instances = { UptimeTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
