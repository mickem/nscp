nscp.print('Loading test script...')

v = nscp.getSetting('NSCA Agent', 'interval', 'broken')
nscp.print('value: ' .. v)

function test_func_query(command, args)
	nscp.print('Inside function (query): ' .. command)
	return 'ok', 'whoops 001', ''
end

function test_func_exec(command, args)
	nscp.print('Inside function (exec): ' .. command)
	return 'ok', 'whoops 002'
end

function test_func_submission(command, args)
	nscp.print('Inside function (exec): ' .. command)
	return 'ok'
end


nscp.execute('version')

local reg = Registry()
reg:simple_function('lua_test', test_func_query, 'this is a command')
reg:simple_cmdline('lua_test', test_func_exec)
reg:simple_subscription('lua_test', test_func_submission)

local settings = Settings()

str = settings:get_string('/settings/lua/scripts', 'testar', 'FOO BAR')
nscp.print('Value: (FOO BAR): ' .. str)
settings:set_string('/settings/lua/scripts', 'testar', 'BAR FOO')
str = settings:get_string('/settings/lua/scripts', 'testar', 'FOO BAR')
nscp.print('Value: (BAR FOO): ' .. str)
i = settings:get_int('/settings/lua/scripts', 'testar', 123)
nscp.print('Value: (123): ' .. i)
settings:set_int('/settings/lua/scripts', 'testar', 456)
i = settings:get_int('/settings/lua/scripts', 'testar', 789)
nscp.print('Value: (456): ' .. i)

local core = Core()
code, msg, perf = core:simple_query('lua_test', {'a', 'b', 'c'})
nscp.print('Value: (query): ' .. code)
nscp.print('Value: (query): ' .. msg)
nscp.print('Value: (query): ' .. perf)
code, msgs = core:simple_exec('*', 'lua_test', {'a', 'b', 'c'})
nscp.print('Value: (exec): ' .. code)
for msg in pairs(msgs) do
	nscp.print('Value: (exec): ' .. msg)
end
code, msg = core:simple_submit('lua_test', 'test_lua', 'ok', 'foo', '')
nscp.print('Value: (submit): ' .. code)
nscp.print('Value: (submit): ' .. msg)

