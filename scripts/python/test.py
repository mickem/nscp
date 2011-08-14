from NSCP import Settings, Registry, Core, log, status, get_alias

core = Core.get()

def get_help(arguments):
	return (status.OK, 'help: Get help')

	
def test_cmd(arguments):
	global prefix
	log('inside test_cmd')
	return (status.OK, 'The command works: %s (%d)'%(prefix, len(arguments)))

def test_channel(channel, command, code, message, perf):
	log('inside test_channel: %s'%channel)
	log('Data: %d %s %s'%(code, message, perf))

prefix = 'py_'
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
	return (status.OK, 'Tests ok')

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
	
def init(alias):
	global prefix
	if alias:
		prefix = '%s_'%alias

	log('Script: test.py with alias: %s from %s'%(alias, get_alias()))

	conf = Settings.get()
	val = conf.get_string('/modules', 'PythonScript', 'foo')

	log('Got it: %s'%val)
	
	log('Testing to register a function')
	reg = Registry.get()
	reg.simple_function('%stest'%prefix, test, 'This is a sample command')
	reg.simple_function('%snormal'%prefix, normal, 'This is a sample command')
	reg.simple_function('%snop'%prefix, no_perf, 'No performance data')
	reg.simple_function('%snom'%prefix, no_msg, 'No performance data')
	reg.simple_function('%snor'%prefix, no_ret, 'No performance data')

	reg.simple_cmdline('help', get_help)
	reg.simple_cmdline('%stest'%prefix, test_cmd)

	reg.simple_subscription('%stest'%prefix, test_channel)

	(ret, list) = core.simple_submit('%stest'%prefix, 'test.py', status.WARNING, 'hello', '')
	
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
	conf.save()

def shutdown():
	log('Unloading script...')
