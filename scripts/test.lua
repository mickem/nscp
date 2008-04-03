--require("luacom")
--require("luacom")
--require("luacom-lua5-13")

nscp.print('Loading test script...')
-- win = loadlib("win32.dll","luaopen_w32")
-- print(win) -- nil
require( 'w32' )
nscp.print(w32)
nscp.print(w32.FindWindow)

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
  --code, msg, perf = inject('CheckCPU','time=5','MaxCrit=5')
  msg = 'hello'
  perf = 'hello'
  code = 'ok'
  print(code .. ': ' .. msg .. ', ' .. perf)
  collectgarbage ()

  return code, 'hello from LUA: ' .. msg, perf
end


function debug (command, args)
    table.foreachi(args, print)
    print ('Command was: ' .. command)
    return 'ok', 'hello'
end
