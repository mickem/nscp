nscp.print('Loading test script...')

v = nscp.getSetting('NSCA Agent', 'interval', 'broken')
nscp.print('value: ' .. v)

function test_func_query(command, args)
    nscp.print('Inside function (query): ' .. command)
    return 'ok', 'whoops 001', "'a label'=30Z;20;30;10;50 'another label'=33Z;20;30;10;50 "
end

function test_func_exec(command, args)
    nscp.print('Inside function (exec): ' .. command)
    return 'ok', 'whoops 002'
end

function test_func_submission(channel, command, result, msgs)
    nscp.print('Inside function (submit): ' .. channel)
    nscp.print('Inside function (submit): ' .. command)
    nscp.print('Inside function (submit): ' .. result)
    for line, perf in pairs(msgs) do
        nscp.print('Inside function (submit): ' .. line)
        nscp.print('Inside function (submit): ' .. perf)
    end
    return 'ok', 'Good stuff'
end

local reg = Registry()
reg:simple_function('lua_test', test_func_query, 'this is a command')
reg:simple_cmdline('lua_test', test_func_exec, 'something')
reg:simple_subscription('lua_test', test_func_submission, 'something else')

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


function on_start()
    nscp.print('on start')
    local core = Core()
    nscp.print('Executing command')
    code, msgs = core:simple_exec('*', 'lua_test', {'a', 'b', 'c'})
    nscp.print('Value: (exec): ' .. code)
    for id, msg in pairs(msgs) do
        nscp.print('Value: (exec): ' .. msg)
    end
    nscp.print('Executing query')
    code, msg, perf = core:simple_query('lua_test', {'a', 'b', 'c'})
    nscp.print('Value: (query): ' .. code)
    nscp.print('Value: (query): ' .. msg)
    nscp.print('Value: (query): ' .. perf)
    nscp.print('Submitting response')
    code, msg = core:simple_submit('lua_test', 'test_lua', code, msg, perf)
    nscp.print('Value: (submit): ' .. tostring(code))
    nscp.print('Value: (submit): ' .. msg)

end
