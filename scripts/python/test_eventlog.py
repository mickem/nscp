from NSCP import Settings, Registry, Core, log, status, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
import plugin_pb2
from types import *
import socket
import unicodedata

class EventLogTest(BasicTest):
	instance = None
	key = ''
	reg = None
	got_response = False
	last_source = None
	last_command = None
	last_status = None
	last_message = None
	last_perfdata = None
	got_simple_response = None
	
	class SingletonHelper:
		def __call__( self, *args, **kw ) :
			if EventLogTest.instance is None :
				object = EventLogTest()
				EventLogTest.instance = object
			return EventLogTest.instance

	getInstance = SingletonHelper()

	def desc(self):
		return 'Testcase for eventlog'

	def title(self):
		return 'EventLog test'

	def setup(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)
		self.reg.simple_subscription('pytest_evlog', EventLogTest.simple_inbox_handler)

	def simple_inbox_handler(channel, source, command, code, message, perf):
		instance = EventLogTest.getInstance()
		return instance.simple_inbox_handler_wrapped(channel, source, command, code, message, perf)
	simple_inbox_handler = Callable(simple_inbox_handler)

	def simple_inbox_handler_wrapped(self, channel, source, command, status, message, perf):
		message = unicodedata.normalize('NFKD', message).encode('ascii','ignore')
		log('Got simple message %s on %s'%(command, channel))
		self.got_simple_response = True
		self.last_source = source
		self.last_command = command
		self.last_status = status
		self.last_message = message
		self.last_perfdata = perf
		return True

	def teardown(self):
		None

	def test_create(self, source, id, severity, category, arguments):
		self.last_message = None
		args = ['--source', source, 
					'--id', id,
					'--severity', severity,
					'--category', category
					]
		for f in arguments:
			args.append('--argument')
			args.append(f)
		(ret, msg) = Core.get().simple_exec('any', 'insert-eventlog', args)
		return ret == 0

	def run_test(self):
		result = TestResult()
		result.add_message(self.test_create('Application Error', 1000, '0', 'error', ['a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a']), 'Testing to create a log message')
		sleep(500)
		log(self.last_message)
		log('%s'%self.last_message)
		result.assert_equals(self.last_message, 'error Application Error: ', 'Verify that message is sent through')
		return result

	def install(self, arguments):
		conf = Settings.get()
		conf.set_string('/modules', 'pytest_eventlog', 'CheckEventLog')
		conf.set_string('/modules', 'pytest', 'PythonScript')

		conf.set_string('/settings/pytest/scripts', 'test_eventlog', 'test_eventlog.py')
		
		conf.set_string('/settings/pytest_eventlog/real-time', 'enabled', 'true')
		conf.set_string('/settings/pytest_eventlog/real-time', 'maximum age', '5s')
		conf.set_string('/settings/pytest_eventlog/real-time', 'destination', 'pytest_evlog')
		conf.set_string('/settings/pytest_eventlog/real-time', 'language', 'english')
		
		conf.save()
	
	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		None
		#reg = Registry.get(plugin_id)
		#reg.simple_function('test_eventlog', test, 'Run python EventLog unit test suite')

	def shutdown(self):
		None

setup_singleton(EventLogTest)

all_tests = [EventLogTest]

def __main__():
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
