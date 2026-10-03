-- Fixture for tests/luascript-api.test.ts.
--
-- Every LUAScript API docs/docs/extending/lua.md promises, each behind a query
-- the test drives over REST (or, for the command-line handlers, through the
-- CLI). A query answers with what the API returned, so the assertions live in
-- the test and this file only has to call things and report.
--
-- Lifecycle calls are appended to the file named by /settings/luaapi/trace,
-- one line each, because the interesting ones (the top-level code running
-- again after a reload, on_start, main under `nscp lua execute`) happen where
-- no query can see them.

local SETTINGS_PATH = '/settings/luaapi'

local seen_submissions = {}
local calls = 0

local function lifecycle(line)
	local trace = nscp.getSetting(SETTINGS_PATH, 'trace', '')
	if trace == '' then return end
	local f = io.open(trace, 'a')
	if f then
		f:write(line .. '\n')
		f:close()
	end
end

-- REST passes each option as one `key=value` token; a bare key is a flag.
local function kv(args)
	local out = {}
	for _, a in ipairs(args) do
		local k, v = string.match(a, '^([^=]*)=(.*)$')
		if k then out[k] = v else out[a] = '' end
	end
	return out
end

-- Arguments minus the ones named, in order, for handing on to another call.
local function without(args, ...)
	local drop = {}
	for _, k in ipairs({...}) do drop[k] = true end
	local out = {}
	for _, a in ipairs(args) do
		local k = string.match(a, '^([^=]*)=') or a
		if not drop[k] then table.insert(out, a) end
	end
	return out
end

-- --- Registry:simple_query / simple_function -----------------------------------

local function lua_echo(command, args)
	return 'ok', 'args: ' .. table.concat(args, '|'), "'count'=" .. #args .. ';5;8'
end

local function lua_status(command, args)
	local o = kv(args)
	local s = o.status or 'ok'
	-- status=2 passes the integer form the docs say is accepted too.
	local code = tonumber(s) or s
	return code, 'status: ' .. s, "'value'=" .. (o.value or '1') .. ';5;8'
end

local function lua_command_name(command, args)
	return 'ok', 'command: ' .. command, ''
end

-- Counts its own calls, so the test can tell whether something ran it.
local function lua_calls(command, args)
	calls = calls + 1
	return 'ok', 'calls=' .. calls, ''
end

-- --- error paths ------------------------------------------------------------------

local function lua_raise(command, args)
	error('boom from lua_raise')
end

local function lua_nothing(command, args)
end

local function lua_bad_code(command, args)
	return 'not a status', 'message', ''
end

local function lua_short(command, args)
	return 'warning'
end

-- --- Core -------------------------------------------------------------------------

local function lua_nested(command, args)
	local o = kv(args)
	local code, msg, perf = Core():simple_query(o.target, without(args, 'target'))
	return code, 'nested ' .. o.target .. ': ' .. msg, perf
end

-- Core:simple_query with a single string argument instead of a table.
local function lua_nested_string(command, args)
	local o = kv(args)
	local code, msg, perf = Core():simple_query(o.target, o.arg)
	return code, 'nested ' .. o.target .. ': ' .. msg, perf
end

local function lua_exec(command, args)
	local o = kv(args)
	local code, lines = Core():simple_exec(o.module, o.command, { o.arg or '' })
	return 'ok', 'exec ' .. o.command .. ': code=' .. tostring(code) .. ' lines=' .. table.concat(lines, '|'), ''
end

local function lua_submit(command, args)
	local o = kv(args)
	local ok, resp = Core():simple_submit(o.channel, o.command or 'lua_submitted', o.status or 'warning',
		o.message or 'from lua', o.perf or '')
	local code = 'critical'
	if ok then code = 'ok' end
	return code, 'submitted: ' .. tostring(ok) .. ' ' .. tostring(resp), ''
end

-- The raw protobuf query: build the request with create_pb_query, run it, and
-- report what came back. Without the Lua protobuf bindings the response can
-- only be measured, not decoded.
local function lua_raw_query(command, args)
	local o = kv(args)
	local core = Core()
	local req = core:create_pb_query(o.target, without(args, 'target'))
	local ok, resp = core:query(req)
	return 'ok', 'raw ' .. o.target .. ': ok=' .. tostring(ok) .. ' request=' .. #req .. ' response=' .. #resp, ''
end

local function lua_reload(command, args)
	local o = kv(args)
	Core():reload(o.name)
	return 'ok', 'reload ' .. o.name .. ' requested', ''
end

local function lua_log(command, args)
	local o = kv(args)
	Core():log(o.level or 'info', o.message or 'core log line')
	nscp.info('nscp.info: ' .. (o.message or ''))
	nscp.print('nscp.print: ' .. (o.message or ''))
	nscp.error('nscp.error: ' .. (o.message or ''))
	return 'ok', 'logged', ''
end

local function lua_sleep(command, args)
	local o = kv(args)
	nscp.sleep(tonumber(o.ms or '0'))
	return 'ok', 'slept ' .. (o.ms or '0'), ''
end

-- The calls the docs list but the wrapper does not implement. Each must raise
-- a Lua error the script can catch, naming the call.
local function lua_unsupported(command, args)
	local o = kv(args)
	local calls = {
		exec = function() return Core():exec('x') end,
		submit = function() return Core():submit('x') end,
		cmdline = function() return Registry():cmdline('x', function() end, 'x') end,
		subscription = function() return Registry():subscription('x', function() end, 'x') end,
	}
	local ok, err = pcall(calls[o.call])
	return 'ok', o.call .. ': ok=' .. tostring(ok) .. ' err=' .. tostring(err), ''
end

-- A call made with too few arguments raises an error naming the syntax.
local function lua_bad_syntax(command, args)
	local ok, err = pcall(function() return Core():simple_query('only_a_command') end)
	return 'ok', 'ok=' .. tostring(ok) .. ' err=' .. tostring(err), ''
end

-- --- subscriptions ------------------------------------------------------------------

local function on_submission(channel, command, code, lines)
	local parts = {}
	for msg, perf in pairs(lines) do table.insert(parts, msg .. '=' .. perf) end
	table.insert(seen_submissions, channel .. '|' .. command .. '|' .. code .. '|' .. table.concat(parts, ';'))
	return true, 'recorded'
end

local function on_reject(channel, command, code, lines)
	return false, 'rejected by lua'
end

local function on_raise(channel, command, code, lines)
	error('boom from on_raise')
end

local function lua_seen(command, args)
	if #seen_submissions == 0 then return 'ok', 'none', '' end
	return 'ok', table.concat(seen_submissions, '\n'), ''
end

-- --- Settings -------------------------------------------------------------------------

local function lua_settings(command, args)
	local s = Settings()
	local o = kv(args)
	if o.set then s:set_string(SETTINGS_PATH, 'scratch', o.set) end
	if o.set_int then s:set_int(SETTINGS_PATH, 'counter', tonumber(o.set_int)) end
	if o.set_bool then s:set_bool(SETTINGS_PATH, 'switch', o.set_bool == 'true') end
	local keys = s:get_section(SETTINGS_PATH)
	table.sort(keys)
	return 'ok', string.format('greeting=%s scratch=%s int=%d bool=%s counter=%d switch=%s keys=%s',
		s:get_string(SETTINGS_PATH, 'greeting', 'unset'),
		s:get_string(SETTINGS_PATH, 'scratch', 'unset'),
		s:get_int(SETTINGS_PATH, 'number', -1),
		tostring(s:get_bool(SETTINGS_PATH, 'flag', false)),
		s:get_int(SETTINGS_PATH, 'counter', -1),
		tostring(s:get_bool(SETTINGS_PATH, 'switch', false)),
		table.concat(keys, ',')), ''
end

-- Typed reads of a value that is not of that type, and of a missing section.
local function lua_settings_bad(command, args)
	local s = Settings()
	return 'ok', string.format('int=%d bool=%s missing=%s getSetting=%s',
		s:get_int(SETTINGS_PATH, 'word', -1),
		tostring(s:get_bool(SETTINGS_PATH, 'word', true)),
		s:get_string('/settings/luaapi/no such section', 'key', 'default'),
		nscp.getSetting(SETTINGS_PATH, 'word', 'unset')), ''
end

local function lua_settings_save(command, args)
	local s = Settings()
	s:set_string(SETTINGS_PATH, 'saved', 'by lua')
	s:save()
	return 'ok', 'saved', ''
end

-- --- command line -----------------------------------------------------------------------

local function lua_cli_echo(command, args)
	return 'ok', 'cli: ' .. table.concat(args, ' ')
end

local function lua_cli_fail(command, args)
	error('boom from lua_cli_fail')
end

local function lua_cli_warn(command, args)
	return 'warning', 'cli warned'
end

-- --- lifecycle ---------------------------------------------------------------------------

function on_start()
	lifecycle('start')
end

function main(args)
	lifecycle('main ' .. table.concat(args, ' '))
	return 'ok', 'main ran with ' .. #args .. ' arguments'
end

-- --- registration (the top-level code) --------------------------------------------------

local s = Settings()
s:register_path(SETTINGS_PATH, 'Lua API fixture', 'Settings the luascript-api fixture registers')
s:register_key(SETTINGS_PATH, 'greeting', 'string', 'Greeting', 'What lua_settings reports', 'hello')

local reg = Registry()
reg:simple_query('lua_echo', lua_echo, 'Echo the arguments back')
reg:simple_function('lua_status', lua_status, 'Return the status named by status=')
reg:simple_query('lua_command_name', lua_command_name, '')
reg:simple_query('lua_calls', lua_calls, 'Count the calls made to it')
reg:simple_query('lua_raise', lua_raise, 'Raise from a handler')
reg:simple_query('lua_nothing', lua_nothing, 'Return nothing from a handler')
reg:simple_query('lua_bad_code', lua_bad_code, 'Return a status that is not one')
reg:simple_query('lua_short', lua_short, 'Return only a status')
reg:simple_query('lua_nested', lua_nested, 'Core:simple_query')
reg:simple_query('lua_nested_string', lua_nested_string, 'Core:simple_query with a string argument')
reg:simple_query('lua_exec', lua_exec, 'Core:simple_exec')
reg:simple_query('lua_submit', lua_submit, 'Core:simple_submit')
reg:simple_query('lua_raw_query', lua_raw_query, 'Core:create_pb_query and Core:query')
reg:simple_query('lua_reload', lua_reload, 'Core:reload')
reg:simple_query('lua_log', lua_log, 'Core:log and the nscp log functions')
reg:simple_query('lua_sleep', lua_sleep, 'nscp.sleep')
reg:simple_query('lua_unsupported', lua_unsupported, 'The calls that are not implemented')
reg:simple_query('lua_bad_syntax', lua_bad_syntax, 'A call with too few arguments')
reg:simple_query('lua_seen', lua_seen, 'What the subscription handlers recorded')
reg:simple_query('lua_settings', lua_settings, 'Settings get/set/get_section')
reg:simple_query('lua_settings_bad', lua_settings_bad, 'Typed reads of the wrong type')
reg:simple_query('lua_settings_save', lua_settings_save, 'Settings:save')

reg:simple_cmdline('lua_cli_echo', lua_cli_echo, 'Echo the arguments back')
reg:simple_cmdline('lua_cli_fail', lua_cli_fail, 'Raise from a command-line handler')
reg:simple_cmdline('lua_cli_warn', lua_cli_warn, 'Answer with a warning')

reg:simple_subscription('LUACHAN', on_submission, 'Record what is submitted')
reg:simple_subscription('LUAREJECT', on_reject, 'Reject every submission')
reg:simple_subscription('LUARAISE', on_raise, 'Raise from a subscription handler')

lifecycle('load')
nscp.info('luaapi fixture loaded')
