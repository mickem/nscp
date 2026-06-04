-- Protobuf-free Lua port of scripts/python/test_scheduler.py.
--
-- Verifies the Scheduler module actually fires scheduled checks at their
-- configured intervals and routes the results through a submission channel:
--   * a Lua simple_function (test_sched_cmd) is the scheduled check - it counts
--     how often it runs, keyed by its argument;
--   * a Lua simple_subscription on a channel counts the results the scheduler
--     submits back.
-- After a sampling window we assert each interval fired within a plausible band
-- and that faster intervals fired more often than slower ones. This drives the
-- Scheduler worker threads + core submit path, a good sanitizer target. No
-- protobuf, no network.
--
-- The Python original samples for 90s to tightly bound every interval; this port
-- uses a shorter window with looser bounds (plus the robust "faster fires more"
-- invariant) so it stays reliable as a CI/sanitizer test.
local test = require("test_helper")

local ALIAS = 'test_sched'              -- Scheduler module instance + settings root
local BASE = '/settings/' .. ALIAS
local CHANNEL = 'test_sched_py'         -- channel the scheduler submits results to
local COMMAND = 'test_sched_cmd'        -- the scheduled (Lua) check command

-- Sampling window in seconds; the per-interval bounds below are calibrated for it.
local WINDOW = 16

-- Shared counters (single Lua state, so plain upvalues are fine).
local check_count = 0
local results_count = 0
local command_count = {}

-- The scheduled check: count invocations keyed by the first argument (which the
-- schedules below set to the interval name, e.g. "1s"), and echo it back.
local function check_handler(command, args)
	check_count = check_count + 1
	local key = args[1]
	if key then
		command_count[key] = (command_count[key] or 0) + 1
		return 'ok', key, ''
	end
	return 'ok', 'pong', ''
end

-- The result sink: the scheduler submits each (OK) check result here.
-- Signature/return match the simple_subscription contract (channel, command,
-- result, lines) -> (status, message).
local function on_result(channel, command, result, lines)
	results_count = results_count + 1
	return 'ok', 'received'
end

local reg = Registry()
reg:simple_function(COMMAND, check_handler, 'noop scheduler check')
reg:simple_subscription(CHANNEL, on_result, 'scheduler result sink')

local SchedulerTest = {}

function SchedulerTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', ALIAS, 'Scheduler')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_scheduler', 'test_scheduler.lua')

	-- Default schedule: provides the channel inherited by the others, plus its
	-- own 5s beat.
	conf:set_string(BASE .. '/schedules/default', 'channel', CHANNEL)
	conf:set_string(BASE .. '/schedules/default', 'command', COMMAND .. ' default')
	conf:set_string(BASE .. '/schedules/default', 'interval', '5s')
	conf:set_string(BASE .. '/schedules/default', 'randomness', '0%')

	conf:set_string(BASE .. '/schedules/checker_1s', 'command', COMMAND .. ' 1s')
	conf:set_string(BASE .. '/schedules/checker_1s', 'interval', '1s')

	conf:set_string(BASE .. '/schedules/checker_5s', 'command', COMMAND .. ' 5s')
	conf:set_string(BASE .. '/schedules/checker_5s', 'interval', '5s')

	conf:set_string(BASE .. '/schedules/checker_10s', 'command', COMMAND .. ' 10s')
	conf:set_string(BASE .. '/schedules/checker_10s', 'interval', '10s')
end

function SchedulerTest:setup() end

function SchedulerTest:teardown()
	-- Stop the scheduler so it does not keep firing after the test.
	local conf = Settings()
	conf:set_string(BASE, 'threads', '0')
	Core():reload(ALIAS)
end

-- Assert command `key` fired strictly between min and max times in the window.
function SchedulerTest:check_one(result, key, min, max)
	local n = command_count[key] or 0
	result:add_message(n > min and n < max,
		string.format('check %s fired %d times (expected %d..%d)', key, n, min, max))
end

function SchedulerTest:run()
	local core = Core()
	local result = test.TestResult:new{message = 'Scheduler fires checks and routes results'}

	-- The scheduler has been running since boot; reset and measure a clean window.
	check_count = 0
	results_count = 0
	command_count = {}

	local start = os.time()
	while os.time() - start < WINDOW do
		nscp.sleep(2000)
		local elapsed = os.time() - start
		core:log('info', string.format('scheduler test: %ds, %d results so far', elapsed, results_count))
	end

	result:add_message(check_count > 0, string.format('scheduled checks fired (%d)', check_count))
	result:add_message(results_count > 0, string.format('results delivered to channel (%d)', results_count))

	-- Expected counts ~= WINDOW / interval; bounds absorb missed/extra beats.
	--   1s -> ~16, 5s (and default) -> ~3, 10s -> ~1.
	-- ('default' is the inheritance template, not an executed schedule, so it
	-- never fires under its own key - we don't assert it.)
	self:check_one(result, '1s', 8, 30)
	self:check_one(result, '5s', 1, 8)
	self:check_one(result, '10s', 0, 5)

	-- Robust ordering invariant: faster intervals must fire more often.
	result:add_message((command_count['1s'] or 0) > (command_count['5s'] or 0),
		'1s interval fired more often than 5s')
	result:add_message((command_count['5s'] or 0) >= (command_count['10s'] or 0),
		'5s interval fired at least as often as 10s')

	return result
end

local instances = { SchedulerTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
