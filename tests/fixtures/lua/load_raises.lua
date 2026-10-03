-- Registers one query and an on_start, then raises from its top-level code.
-- The suite checks the error is logged, the other scripts load, what was
-- registered before the error stays, and on_start never runs for a script
-- that did not load.
Registry():simple_query('lua_before_raise', function(command, args)
	return 'ok', 'registered before the raise', ''
end, 'Registered before the top-level code raised')

function on_start()
	local trace = nscp.getSetting('/settings/luaapi', 'trace', '')
	local f = io.open(trace, 'a')
	if f then
		f:write('start load_raises\n')
		f:close()
	end
end

error('boom from the top-level code')
