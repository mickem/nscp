-- Does not parse: the suite checks the module logs it and loads the others.
Registry():simple_query('lua_never', function(command, args) return 'ok', 'never', '' end, 'Never registered'
