from NSCP import Settings, Registry, Core, log, status
from test_helper import BasicTest, TestResult, install_testcases, init_testcases, shutdown_testcases


# Round trip through the simple scripting API inside one PythonScript instance:
# Core.simple_submit on a channel reaches the script's simple_subscription, and
# Core.simple_query reaches its simple_function. Status, message and perfdata
# must survive both legs unchanged.
class ChannelTest(BasicTest):

	channel = ''
	reg = None
	core = None
	conf = None

	last_channel = ''
	last_command = ''
	last_status = status.UNKNOWN
	last_message = ''
	last_perf = ''

	def title(self):
		return 'Channel Test'

	def desc(self):
		return 'Testing that channels work'

	@staticmethod
	def submission_handler(channel, source, command, code, message, perf):
		log('Got message on %s'%channel)
		instance.set_last(channel, command, code, message, perf)

	@staticmethod
	def command_handler(arguments):
		return (instance.last_status, '%s'%instance.last_message, '%s'%instance.last_perf)

	def init(self, plugin_id):
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def setup(self, plugin_id, prefix):
		self.channel = '_%stest_channel'%prefix
		self.reg.simple_subscription(self.channel, ChannelTest.submission_handler)
		self.reg.simple_function(self.channel, ChannelTest.command_handler, 'This is a sample command')

	def reset_last(self):
		self.last_channel = None
		self.last_command = None
		self.last_status = None
		self.last_message = None
		self.last_perf = None

	def set_last(self, channel, command, code, message, perf):
		self.last_channel = channel
		self.last_command = command
		self.last_status = code
		self.last_message = message
		self.last_perf = perf

	def test_simple(self, command, code, message, perf, tag):
		result = TestResult('Channel round trip: %s'%tag)
		self.reset_last()
		(ret, msg) = self.core.simple_submit(self.channel, '%s'%command, code, '%s'%message, '%s'%perf)
		result.add_message(ret, 'Submitted on the channel: %s'%tag, msg)
		result.assert_equals(self.last_status, code, 'Submit: return code')
		result.assert_equals(self.last_message, message, 'Submit: message')
		result.assert_equals(self.last_perf, perf, 'Submit: performance data')

		self.set_last('', '', code, message, perf)
		(retcode, retmessage, retperf) = self.core.simple_query(self.channel, [])
		result.assert_equals(retcode, code, 'Query: return code')
		result.assert_equals(retmessage, message, 'Query: message')
		result.assert_equals(retperf, perf, 'Query: performance data')
		return result

	def run_test(self, cases = None):
		result = TestResult('Channel Test')
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

	def install(self):
		self.conf.set_string('/modules', 'pytest', 'PythonScript')
		self.conf.set_string('/settings/pytest/scripts', 'test_channels', 'test_channels.py')
		self.conf.save()


instance = ChannelTest()
all_tests = [instance]

def __main__(args):
	install_testcases(all_tests)

def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
