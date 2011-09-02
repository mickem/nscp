from NSCP import Settings, Registry, Core, log, status
#import sys
#sys.path.append('D:/source/nscp/build/x64/scripts/python/include')

import plugin_pb2

core = Core.get()

prefix = 'py_'

def get_help(arguments):
	return (status.OK, 'help: Get help')

	
def test_cmd(arguments):
	global prefix
	log('inside test_cmd')
	return (status.OK, 'The command works: %s (%d)'%(prefix, len(arguments)))

def test_channel(channel, command, code, message, perf):
	global prefix
	log('inside test_channel: %s with prefix %s'%(channel, prefix))
	log('Data: %d %s %s'%(code, message, perf))

def test(arguments):
	global prefix
	log('inside test')
	for a in arguments:
		log('Got argument: %s'%a)
	(retcode, msg, perf) = core.simple_query("%snormal"%prefix, [])
	if msg != "ok got: ":
		return (status.CRITICAL, "Test failed")
	(retcode, msg, perf) = core.simple_query("%snormal"%prefix, ["hello"])
	if msg != "ok got: hello":
		return (status.CRITICAL, "Test failed")
	(retcode, msg, perf) = core.simple_query("%snormal"%prefix, ["hello", "world"])
	if perf != "'args'=2":
		return (status.CRITICAL, "Test failed: -%s-"%perf)
		
	message = plugin_pb2.QueryRequestMessage()
	
	message.header.version = plugin_pb2.Common.VERSION_1

	payload = message.payload.add()
	payload.command = "%snormal"%prefix
	payload.arguments.append("hello")

	(retcode, buffer) = core.query("%snormal"%prefix, message.SerializeToString())
	message = plugin_pb2.QueryResponseMessage()
	message.ParseFromString(buffer)
	if message.payload[0].perf[0].alias != "args":
		return (status.CRITICAL, "Test failed: -%s-"%message.payload[0].perf[0].alias)
	if message.payload[0].perf[0].float_value.value != 1:
		return (status.CRITICAL, "Test failed: -%s-"%message.payload[0].perf[0].float_value.value)
	
	return (status.OK, 'All tests ok')

def normal(arguments):
	log('inside normal')
	for a in arguments:
		log(' | Got argument: %s'%a)
	if arguments:
		return (status.OK, 'ok got: %s'%arguments[0], "args=%d; "%len(arguments))
	return (status.OK, 'ok got: ', "args=%d; "%len(arguments))
	
def no_perf(arguments):
	log('inside no_perf')
	for a in arguments:
		log('Got argument: %s'%a)
	return (status.WARNING, 'I am ok')

def no_msg(arguments):
	log('inside no_perf')
	for a in arguments:
		log('Got argument: %s'%a)
	return (1)

def no_ret(arguments):
	log('inside no_perf')
	for a in arguments:
		log('Got argument: %s'%a)
	
def simple_pb(command, buffer):
	message = plugin_pb2.QueryRequestMessage()
	message.ParseFromString(buffer)
	for p in message.payload:
		log('Command: %s'%p.command)
		for a in p.arguments:
			log('Arg: %s'%a)
	
	response = plugin_pb2.QueryResponseMessage()
	response.header.version = plugin_pb2.Common.VERSION_1

	payload = response.payload.add()
	payload.command = command
	payload.result = plugin_pb2.Common.OK
	payload.message = 'This is more difficult...'

	return (status.OK, response.SerializeToString())

def init(plugin_id, plugin_alias, script_alias):
	global prefix
	if script_alias:
		prefix = '%s_'%script_alias

	log('Script: test.py with alias: %s (%s:%d)'%(script_alias, plugin_alias, plugin_id))

	conf = Settings.get()
	val = conf.get_string('/modules', 'PythonScript', 'foo')

	log('Got it: %s'%val)
	
	log('Testing to register a function')
	reg = Registry.get(plugin_id)
	reg.simple_function('%stest'%prefix, test, 'This is a sample command')
	reg.simple_function('%snormal'%prefix, normal, 'This is a sample command')
	reg.simple_function('%snop'%prefix, no_perf, 'No performance data')
	reg.simple_function('%snom'%prefix, no_msg, 'No performance data')
	reg.simple_function('%snor'%prefix, no_ret, 'No performance data')
	
	reg.function('%spb'%prefix, simple_pb, 'Simple protocolbuffer sample')
	

	reg.simple_cmdline('help', get_help)
	reg.simple_cmdline('%stest'%prefix, test_cmd)

	reg.simple_subscription('%stest'%prefix, test_channel)

	core.simple_submit('%stest'%prefix, 'test.py', status.WARNING, 'hello', '')
	
	(ret, list) = core.simple_exec('%stest'%prefix, ['a', 'b', 'c'])
	for l in list:
		log('-- %s --'%l)

	log('Testing to register settings keys')
	conf.register_path('hello', 'PYTHON SETTINGS', 'This is stuff for python')
	conf.register_key('hello', 'python', 'int', 'KEY', 'This is a key', '42')

	log('Testing to get key (nonexistant): %d' % conf.get_int('hello', 'python', -1))
	conf.set_int('hello', 'python', 4)
	log('Testing to get it (after setting it): %d' % conf.get_int('hello', 'python', -1))

	log('Saving configuration...')
	#conf.save()

def shutdown():
	log('Unloading script...')
