
print('Loading test script...')

register_command('check_something', 'something')

function something (command)
  print(command)
  code, msg, perf = inject('CheckCPU','time=5','MaxCrit=5')
  print(code .. ': ' .. msg .. ', ' .. perf)
  return code, 'hello from LUA: ' .. msg, perf
end
