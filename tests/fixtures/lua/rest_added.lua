-- Added through PUT /api/v2/scripts/lua and `nscp lua add` by the suite.
Registry():simple_query('lua_rest_added', function(command, args)
	return 'ok', 'rest added', ''
end, 'Added by the scripts API')
