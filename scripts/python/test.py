from NSCP import Settings, Registry, Core, log, status
from test_helper import Callable, run_tests
import plugin_pb2

core = Core.get()

prefix = 'py_'
plugin_id = 0

def get_help(arguments):
	return (status.OK, 'help: Get help')

class ChannelTest:
	instance = None
	channel = ''
	reg = None
	
	last_channel = ''
	last_command = ''
	last_status = status.UNKNOWN
	last_message = ''
	last_perf = ''

	class SingletonHelper:
		def __call__( self, *args, **kw ) :
			if ChannelTest.instance is None :
				object = ChannelTest()
				ChannelTest.instance = object
			return ChannelTest.instance

	getInstance = SingletonHelper()

	def desc(self):
		return 'Testing that channels work'

	def test_submission_handler_001(channel, command, code, message, perf):
		instance = ChannelTest.getInstance()
		instance.last_channel = channel
		instance.last_command = command
		instance.last_status = code
		instance.last_message = message
		instance.last_perf = perf
	test_submission_handler_001 = Callable(test_submission_handler_001)
		
	def test_command_handler_001(arguments):
		instance = ChannelTest.getInstance()
		return (instance.last_status, instance.last_message, instance.last_perf)
	test_command_handler_001 = Callable(test_command_handler_001)

	def setup(self, plugin_id, prefix):
		self.channel = '_%stest_channel'%prefix
		self.reg = Registry.get(plugin_id)
		self.reg.simple_subscription('%s_001'%self.channel, ChannelTest.test_submission_handler_001)
		self.reg.simple_function('%s_001'%self.channel, ChannelTest.test_command_handler_001, 'This is a sample command')
		
	def teardown(self):
		None
		#self.reg.unregister_simple_subscription('%s_001'%self.channel)
		#self.reg.unregister_simple_function('%s_001'%self.channel)

	def test_simple(self, channel, command, code, message, perf, tag):
		core.simple_submit('%s'%channel, '%s'%command, code, '%s'%message, '%s'%perf)
		(retcode, retmessage, retperf) = core.simple_query(channel, [])
		isok = True
		if retcode != code:
			log('FAILED - Test did not get the correct retuirn code: %s = %s (%s)'%(retcode, code, retmessage))
			isok = False
		if retmessage != message:
			log('FAILED - Test did not get the correct retuirn code: %s = %s'%(retmessage, message))
			isok = False
		if retperf != perf:
			log('FAILED - Test did not get the correct retuirn code: %s = %s'%(retperf, perf))
			isok = False
		if isok:
			log('OK - Test successfull: %s'%tag)
			return 0
		return 1
		

	def run_test(self):
		count = 0
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.OK, 'qwerty', '', 'simple ok')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.WARNING, 'foobar', '', 'simple warning')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.CRITICAL, 'test', '', 'simple critical')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.UNKNOWN, '1234567890', '', 'simple unknown')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.OK, 'qwerty', "'foo'=5%", 'simple performance data 001')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.OK, 'qwerty', "'foo'=5%;10", 'simple performance data 002')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.OK, 'qwerty', "'foo'=5%;10;23", 'simple performance data 003')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.OK, 'qwerty', "'foo'=5%;10;23;10;78", 'simple performance data 004')
		count += self.test_simple('%s_001'%self.channel, 'foobar', status.OK, 'qwerty', "'foo'=5%;10;23;10;78 'bar'=1k;2;3", 'simple performance data 005')
		if count > 0:
			log("ERROR: %d tests failed"%count)
		else:
			log("OK: all tests successfull")
		return (count, 9)


class CommandTest:
	instance = None
	key = ''
	reg = None
	
	class SingletonHelper:
		def __call__( self, *args, **kw ) :
			if CommandTest.instance is None :
				object = CommandTest()
				CommandTest.instance = object
			return CommandTest.instance

	getInstance = SingletonHelper()

	def desc(self):
		return 'Testing that channels work'

	def test_command_handler_001(arguments):
		if len(arguments) == 0:
			return (status.OK, 'no-arguments', '')
		retcode = status.UNKNOWN
		message = ''
		perf = ''
		if len(arguments) > 0:
			if arguments[0] == 'OK':
				retcode = status.OK
			elif arguments[0] == 'WARNING':
				retcode = status.WARNING
			elif arguments[0] == 'CRITICAL':
				retcode = status.CRITICAL
			elif arguments[0] == 'UNKNOWN':
				retcode = status.UNKNOWN
			message = 'arg-count: %d'%len(arguments)
		return (retcode, message, perf)
	test_command_handler_001 = Callable(test_command_handler_001)

	def setup(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)
		self.reg.simple_function('%s_001'%self.key, CommandTest.test_command_handler_001, 'This is a sample command')
		
	def teardown(self):
		None
		#self.reg.unregister_simple_function('%s_001'%self.key)

	def test_simple(self, key, args, code, message, perf, tag):
		(retcode, retmessage, retperf) = core.simple_query(key, args)
		isok = True
		if code and retcode != code:
			log('FAILED - Test did not return correct values (code): %s = %s (%s)'%(retcode, code, retmessage))
			isok = False
		if message and retmessage != message:
			log('FAILED - Test did not return correct values (message): %s = %s'%(retmessage, message))
			isok = False
		if perf and retperf != perf:
			log('FAILED - Test did not return correct values (perf): %s = %s'%(retperf, perf))
			isok = False
		if isok:
			log('OK - Test successfull: %s'%tag)
			return 0
		return 1
		

	def run_test(self):
		count = 0
		key001 = '%s_001'%self.key
		count += self.test_simple(key001, [], status.OK, 'no-arguments', '', 'simple check')
		count += self.test_simple(key001, ['OK'], status.OK, 'arg-count: 1', None, 'simple check: Ok')
		count += self.test_simple(key001, ['WARNING'], status.WARNING, 'arg-count: 1', None, 'simple check: Warn')
		count += self.test_simple(key001, ['UNKNOWN'], status.UNKNOWN, 'arg-count: 1', None, 'simple check: Unknown')
		count += self.test_simple(key001, ['CRITICAL'], status.CRITICAL, 'arg-count: 1', None, 'simple check: Crit')
		if count > 0:
			log("ERROR: %d tests failed"%count)
		else:
			log("OK: all tests successfull")
		return (count, 9)

def test(arguments):
	global prefix
	global plugin_id

	(all_failed, all_count) = run_tests([ChannelTest, CommandTest])
	if all_failed == 0:
		return (status.OK, 'All tests ok: %d'%all_count)
	else:
		return (status.CRITICAL, 'Tests failed %d of %d'%(all_failed, all_count))

def install_test(arguments):
	log('-+---==(TEST INSTALLER)==---------------------------------------------------+-')
	log(' | Setup nessecary configuration for running test                           |')
	log(' | This includes: Loading the PythonScript module at startup                |')
	log(' | To use this please run nsclient++ in "test mode" like so:                |')
	log(' | nscp --test                                                              |')
	log(' | Then start the pytest_test command by typing it and press enter like so: |')
	log(' | pytest_test                                                              |')
	log(' | Lastly exit by typing exit like so:                                      |')
	log(' | exit                                                                     |')
	log('-+--------------------------------------------------------==(DAS ENDE!)==---+-')
	conf = Settings.get()
	conf.set_string('/modules', 'pytest', 'PythonScript')
	conf.set_string('/settings/pytest/scripts', 'pytest', 'test.py')
	conf.save()

def __main__():
	install_test([])
	
def init(pid, plugin_alias, script_alias):
	global prefix
	global plugin_id
	plugin_id = pid
	if script_alias:
		prefix = '%s_'%script_alias

	conf = Settings.get()
	#val = conf.get_string('/modules', 'PythonScript', 'foo')

	#log('Got it: %s'%val)
	
	log('Testing to register a function')
	reg = Registry.get(plugin_id)
	
	reg.simple_cmdline('help', get_help)
	reg.simple_cmdline('install_python_test', install_test)

	reg.simple_function('%stest'%prefix, test, 'Run python unittest')

	#core.simple_submit('%stest'%prefix, 'test.py', status.WARNING, 'hello', '')
	#core.simple_submit('test', 'test.py', status.WARNING, 'hello', '')
	
	#(ret, list) = core.simple_exec('%stest'%prefix, ['a', 'b', 'c'])
	#for l in list:
	#	log('-- %s --'%l)

	#log('Testing to register settings keys')
	#conf.register_path('hello', 'PYTHON SETTINGS', 'This is stuff for python')
	#conf.register_key('hello', 'python', 'int', 'KEY', 'This is a key', '42')

	#log('Testing to get key (nonexistant): %d' % conf.get_int('hello', 'python', -1))
	#conf.set_int('hello', 'python', 4)
	#log('Testing to get it (after setting it): %d' % conf.get_int('hello', 'python', -1))

	#log('Saving configuration...')
	#conf.save()

def shutdown():
	log('Unloading script...')
