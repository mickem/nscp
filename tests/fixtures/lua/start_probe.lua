-- on_start queries CheckHelpers and records what came back, so the suite can
-- tell whether on_start ran before or after a module the same reload enabled.
local function trace(line)
	local f = io.open(nscp.getSetting('/settings/startprobe', 'trace', ''), 'a')
	if f then
		f:write(line .. '\n')
		f:close()
	end
end

function on_start()
	local code, msg = Core():simple_query('check_ok', { 'message=probe' })
	trace('start ' .. code .. ' ' .. msg)
end

Registry():simple_query('probe_reload', function(command, args)
	Core():reload('service')
	return 'ok', 'service reload requested', ''
end, 'Reload the service')
