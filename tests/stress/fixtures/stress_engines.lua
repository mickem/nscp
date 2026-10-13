-- Lua half of the stress harness's engine fixture (tests/stress/lib/engines.ts).
--
-- Same contract as stress_engines.py: every answer names the engine and echoes
-- its token, and the slow handlers hold a marker file while they run. Lua has
-- one lock for every script in the process; nscp.sleep and the Core calls
-- release it, so a parked or nested Lua check must not stop the others. A Lua
-- check that spins without calling either holds it by design and serialises
-- every Lua check behind it - that is a known limit, not something the
-- scenarios assert on.

local marker_dir = nscp.getSetting('/settings/stress', 'markers', '')

local function token(args)
	return args[1] or '-'
end

local function mark(name)
	if marker_dir == '' then return nil end
	local path = marker_dir .. '/' .. name .. '.running'
	local f = io.open(path, 'w')
	if f then
		f:write('running\n')
		f:close()
	end
	return path
end

local function unmark(path)
	if path then os.remove(path) end
end

local function lua_ok(command, args)
	return 'ok', 'lua ' .. token(args), ''
end

local function lua_sleep(command, args)
	local ms = tonumber(args[1] or '0') or 0
	local m = mark(args[2] or 'lua_sleep')
	nscp.sleep(ms)
	unmark(m)
	return 'ok', 'lua slept ' .. ms, ''
end

-- Lua -> core -> Python: the Lua lock is released around the core call.
local function lua_via_py(command, args)
	local code, msg, perf = Core():simple_query('py_ok', { token(args) })
	return code, 'lua>' .. msg, perf
end

-- Lua -> core -> C++ module.
local function lua_via_cpp(command, args)
	local code, msg, perf = Core():simple_query('check_ok', { 'message=' .. token(args) })
	return code, 'lua>cpp ' .. msg, perf
end

-- Lua waits on a slow Python check through the core.
local function lua_via_py_sleep(command, args)
	local m = mark(args[2] or 'lua_via_py_sleep')
	local code, msg, perf = Core():simple_query('py_sleep', { args[1] or '0' })
	unmark(m)
	return code, 'lua>' .. msg, perf
end

local reg = Registry()
reg:simple_query('lua_ok', lua_ok, 'Answer OK with the token')
reg:simple_query('lua_sleep', lua_sleep, 'nscp.sleep for the given ms')
reg:simple_query('lua_via_py', lua_via_py, 'Lua -> core -> py_ok')
reg:simple_query('lua_via_cpp', lua_via_cpp, 'Lua -> core -> check_ok')
reg:simple_query('lua_via_py_sleep', lua_via_py_sleep, 'Lua -> core -> py_sleep')
