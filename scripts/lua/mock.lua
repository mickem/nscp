function mock_query(command, args)
    return 'ok', command .. "::" .. table.concat(args, ","), "'a label'=30Z;20;30;10;50 'another label'=33Z;20;30"
end

function mock_exec(command, args)
    nscp.print('Inside function (exec): ' .. command)
    return 'ok', 'whoops 002'
end

function mock_submission(channel, command, result, msgs)
    return 'ok', 'Everything is fine'
end

local reg = Registry()
reg:simple_function('mock_query', mock_query, 'Mock query used during tests')
reg:simple_cmdline('mock_exec', mock_exec, 'Mock command used during tests')
reg:simple_subscription('mock_submission', mock_submission, 'Mock submission used during tests')
