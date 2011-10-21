from NSCP import Settings, Registry, Core, log, status
from test_helper import Callable, TestResult, get_test_manager, create_test_manager
from types import *

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

	def title(self):
		return 'Channel Test'

	def desc(self):
		return 'Testing that channels work'

	def test_submission_handler_001(channel, source, command, code, message, perf):
		log('Got messgae on %s'%channel)
		instance = ChannelTest.getInstance()
		instance.set_last(channel, command, code, message, perf)
	test_submission_handler_001 = Callable(test_submission_handler_001)
		
	def test_command_handler_001(arguments):
		instance = ChannelTest.getInstance()
		return (instance.last_status, '%s'%instance.last_message, '%s'%instance.last_perf)
	test_command_handler_001 = Callable(test_command_handler_001)

	def setup(self, plugin_id, prefix):
		self.channel = '_%stest_channel'%prefix
		self.reg = Registry.get(plugin_id)
		self.reg.simple_subscription(self.channel, ChannelTest.test_submission_handler_001)
		self.reg.simple_function(self.channel, ChannelTest.test_command_handler_001, 'This is a sample command')
		
	def teardown(self):
		None
		#self.reg.unregister_simple_subscription('%s_001'%self.channel)
		#self.reg.unregister_simple_function('%s_001'%self.channel)

	def reset_last(self):
		self.last_channel = None
		self.last_command = None
		self.last_status = None
		self.last_message = None
		self.last_perf = None

	def set_last(self, channel, command, status, message, perf):
		self.last_channel = channel
		self.last_command = command
		self.last_status = status
		self.last_message = message
		self.last_perf = perf
		
	def test_simple(self, command, code, message, perf, tag):
		result = TestResult()
		core = Core.get()
		self.reset_last()
		(ret, msg) = core.simple_submit(self.channel, '%s'%command, code, '%s'%message, '%s'%perf)
		result.add_message(ret, 'Testing channels: %s'%tag, msg)
		r1 = TestResult()
		r1.assert_equals(self.last_status, code, 'Return code')
		r1.assert_equals(self.last_message, message, 'Message')
		r1.assert_equals(self.last_perf, perf, 'Performance data')
		result.add(r1)
		
		self.set_last('', '', code, message, perf)
		(retcode, retmessage, retperf) = core.simple_query(self.channel, [])
		result.add_message(True, 'Testing queries: %s'%tag)
		r2 = TestResult()
		r2.assert_equals(self.last_status, code, 'Return code')
		r2.assert_equals(self.last_message, message, 'Message')
		r2.assert_equals(self.last_perf, perf, 'Performance data')
		result.add(r2)
		return result
		

	def run_test(self):
		result = TestResult()
		result.add(self.test_simple('foobar', status.OK, 'qwerty', '', 'simple ok'))
		result.add(self.test_simple('foobar', status.WARNING, 'foobar', '', 'simple warning'))
		result.add(self.test_simple('foobar', status.CRITICAL, 'test', '', 'simple critical'))
		result.add(self.test_simple('foobar', status.UNKNOWN, '1234567890', '', 'simple unknown'))
		result.add(self.test_simple('foobar', status.OK, 'qwerty', "'foo'=5%", 'simple performance data 001'))
		result.add(self.test_simple('foobar', status.OK, 'qwerty', "'foo'=5%;10", 'simple performance data 002'))
		result.add(self.test_simple('foobar', status.OK, 'qwerty', "'foo'=5%;10;23", 'simple performance data 003'))
		result.add(self.test_simple('foobar', status.OK, 'qwerty', "'foo'=5%;10;23;10;78", 'simple performance data 004'))
		result.add(self.test_simple('foobar', status.OK, 'qwerty', "'foo'=5%;10;23;10;78 'bar'=1k;2;3", 'simple performance data 005'))
		return result

	def install(self, arguments):
		conf = Settings.get()
		conf.set_string('/modules', 'pytest', 'PythonScript')

		conf.set_string('/settings/pytest/scripts', 'test_pb', 'test_pb.py')
		
		conf.save()
	
	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		None

	def shutdown(self):
		None


all_tests = [ChannelTest]

def __main__():
	test_manager = create_test_manager()
	test_manager.add(all_tests)
	test_manager.install()
	
def init(plugin_id, plugin_alias, script_alias):
	test_manager = create_test_manager(plugin_id, plugin_alias, script_alias)
	test_manager.add(all_tests)

	test_manager.init()

def shutdown():
	test_manager = get_test_manager()
	test_manager.shutdown()
