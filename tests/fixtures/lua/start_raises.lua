-- on_start raises. The suite checks it is logged, what the script registered
-- still answers, and the other scripts start.
Registry():simple_query('lua_start_raises', function(command, args)
	return 'ok', 'still registered', ''
end, 'Registered by a script whose on_start raises')

function on_start()
	error('boom from on_start')
end
