from NSCP import Settings, Registry, Core, log, status
from types import *
#import sys
#sys.path.append('D:/source/nscp/build/x64/scripts/python/include')

import plugin_pb2

core = Core.get()

prefix = 'py_'
plugin_id = 0

def get_help(arguments):
	return (status.OK, 'help: Get help')

class Callable:
	def __init__(self, anycallable):
		self.__call__ = anycallable

class ChannelTest:
	instance = None
	channel = ''
	reg = None
	
	last_channel = ''
	last_command = ''
	last_status = status.UNKNOWN
	last_message = ''
	last_perf = ''

	instance = None
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
		return (instance.last_status, '%s'%instance.last_message, '%s'%instance.last_perf)
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
		
	
def test_cmd(arguments):
	global prefix
	log('inside test_cmd')
	return (status.OK, 'The command works: %s (%d)'%(prefix, len(arguments)))

def test_channel(channel, command, code, message, perf):
	global prefix
	log('inside test_channel: %s with prefix %s'%(channel, prefix))
	log('Data: %d %s %s'%(code, message, perf))

def run_test(cls):
	instance = cls.getInstance()
	instance.setup(plugin_id, prefix)
	ret = instance.run_test()
	instance.teardown()
	log('Tested %s (%s of %s)'%(instance.desc(), ret[0], ret[1]))
	return ret

def run_tests(list):
	all_failed = 0
	all_count = 0
	for c in list:
		(failed, count) = run_test(c)
		all_failed += failed
		all_count += count
	return (all_failed, all_count)

def test(arguments):
	global prefix
	global plugin_id

	run_tests([ChannelTest])

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

def init(pid, plugin_alias, script_alias):
	global prefix
	global plugin_id
	plugin_id = pid
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
	core.simple_submit('test', 'test.py', status.WARNING, 'hello', '')
	
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
