nscp.print('Loading test script...')

nscp.execute('version')
v = nscp.getSetting('NSCA Agent', 'interval', 'broken')
nscp.print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')
nscp.print('value: ' .. v)
nscp.print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')

nscp.register('check_something', 'something')
nscp.register('lua_debug', 'debug')
nscp.register('foo', 'something')
nscp.register('lua_alias', 'execute_all_alias')

function something (command)
  nscp.print(command)
  code, msg, perf = nscp.execute('CheckCPU','time=5','MaxCrit=5')
  print(code .. ': ' .. msg .. ', ' .. perf)
  collectgarbage ()

  return code, 'hello from LUA: ' .. msg, perf
end

function execute_all_alias()
	commands = nscp.getSection('External Alias')
	ok = 0
	err = 0
	for i,key in pairs(commands) do 
		args = nscp.getSetting('External Alias', key)
		code, msg, perf = nscp.execute(key,args)
		if code == 'ok' then
			ok = ok + 1
		else
			err = err + 1
			print('[' .. i .. '] ' .. key .. ': ' .. code .. ' <' .. msg ..'>')
		end
	end
	if err == 0 then
		return 'ok', 'All ' .. ok .. ' commands were ok'
	else
		return 'error', 'Only ' .. ok .. ' commands of the ' .. (ok+err) .. ' were successfull'
	end
end

function debug (command, args)
    table.foreachi(args, print)
    print ('Command was: ' .. command)
    return 'ok', 'hello'
end
