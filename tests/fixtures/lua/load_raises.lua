-- Registers one query, then raises from its top-level code. The suite checks
-- the error is logged and the other scripts load.
Registry():simple_query('lua_before_raise', function(command, args)
	return 'ok', 'registered before the raise', ''
end, 'Registered before the top-level code raised')
error('boom from the top-level code')
