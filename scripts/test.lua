nscp.print('Loading test script...')

nscp.execute('version')
v = nscp.getSetting('NSCA Agent', 'interval', 'broken')
nscp.print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')
nscp.print('value: ' .. v)
nscp.print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')

nscp.register('check_something', 'something')
nscp.register('lua_debug', 'debug')
nscp.register('foo', 'something')

function something (command)
  nscp.print(command)
  code, msg, perf = nscp.execute('CheckCPU','time=5','MaxCrit=5')
  print(code .. ': ' .. msg .. ', ' .. perf)
  collectgarbage ()

  return code, 'hello from LUA: ' .. msg, perf
end


function debug (command, args)
    table.foreachi(args, print)
    print ('Command was: ' .. command)
    return 'ok', 'hello'
end
