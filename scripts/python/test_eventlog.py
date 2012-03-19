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
	message_count = 0
	
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
		self.message_count = self.message_count + 1
		self.last_perfdata = perf
		return True

	def teardown(self):
		None

	def test_create(self, source, id, level, severity, category, facility, arguments):
		self.last_message = None
		self.message_count = 0
		args = ['--source', source, 
					'--id', id,
					'--level', level,
					'--severity', severity,
					'--category', category,
					'--facility', facility
					]
		for f in arguments:
			args.append('--argument')
			args.append(f)
		(ret, msg) = Core.get().simple_exec('any', 'insert-eventlog', args)
		return ret == 0
		
		
	def test_w_expected(self, filter, syntax, expected):
		result = TestResult('Validating filter: %s'%filter)
		(res, msg, perf) = Core.get().simple_query('CheckEventLog', ['file=Application', 'debug=true', 'warn=ne:%d'%expected, 'crit=ne:%d'%expected, 'filter=%s'%filter, 'syntax=%s'%syntax])
		result.assert_equals(res, status.OK, "Validate status OK for %s"%filter)
		(res, msg, perf) = Core.get().simple_query('CheckEventLog', ['file=Application', 'debug=true', 'warn=eq:%d'%expected, 'crit=ne:%d'%expected, 'filter=%s'%filter, 'syntax=%s'%syntax])
		result.assert_equals(res, status.WARNING, "Validate status OK for %s"%filter)
		(res, msg, perf) = Core.get().simple_query('CheckEventLog', ['file=Application', 'debug=true', 'warn=eq:%d'%expected, 'crit=eq:%d'%expected, 'filter=%s'%filter, 'syntax=%s'%syntax])
		result.assert_equals(res, status.CRITICAL, "Validate status CRIT for %s"%filter)
		return result

	def test_syntax(self, filter, syntax, expected):
		result = TestResult('Validating syntax: %s'%syntax)
		(res, msg, perf) = Core.get().simple_query('CheckEventLog', ['file=Application', 'warn=ne:1', 'filter=%s'%filter, 'syntax=%s'%syntax, 'descriptions'])
		result.assert_equals(msg, expected, "Validate message rendering syntax: %s"%msg)
		return result
		
	def run_test(self):
		result = TestResult('Checking CheckEventLog')
		cache = TestResult('Checking CheckEventLog CACHE')
		
		(res, msg, perf) = Core.get().simple_query('CheckEventLogCACHE', ['warn=eq:1', 'crit=eq:2'])
		cache.assert_equals(res, status.OK, "Validate cache is empty")
		cache.assert_equals(msg, 'Eventlog check ok', "Validate cache is ok: %s"%msg)
		
		
		a_list = ['a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a']
		result.add_message(self.test_create('Application Error', 1000, 'error', 0, 0, 0, a_list), 'Testing to create a log message')
		sleep(500)
		result.assert_equals(self.last_message, 'error Application Error: ', 'Verify that message is sent through')
		result.assert_equals(self.message_count, 1, 'Verify that onlyt one message is sent through')

		result.add_message(self.test_create('Application Error', 1000, 'info', 2, 1, 5, a_list), 'Testing to create a log message')
		sleep(500)
		result.assert_equals(self.last_message, 'info Application Error: ', 'Verify that message is sent through')
		result.assert_equals(self.message_count, 1, 'Verify that onlyt one message is sent through')

		(res, msg, perf) = Core.get().simple_query('CheckEventLogCACHE', ['warn=eq:1', 'crit=eq:2'])
		cache.assert_equals(res, status.CRITICAL, "Validate cache has items")
		cache.assert_equals(msg, 'error Application Error: , info Application Error: , eventlog: 2 = critical', "Validate cache is ok: %s"%msg)
		cache.assert_equals(perf, "'eventlog'=2;1;2", "Validate cache is ok: %s"%msg)
		(res, msg, perf) = Core.get().simple_query('CheckEventLogCACHE', ['warn=eq:1', 'crit=eq:2'])
		cache.assert_equals(res, status.OK, "Validate cache is empty (again)")
		cache.assert_equals(msg, 'Eventlog check ok', "Validate cache is ok: %s"%msg)
		
		result.add(cache)
		
		r = TestResult('Checking filters')
		r.add(self.test_w_expected('id = 1000 and generated gt 1m', '%generated%', 0))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m', '%generated%', 2))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m and id = 1000', '%generated%: %id%, %category%', 2))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m and category = 1', '%category%', 1))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m and category = 0', '%category%', 1))
		r.add(self.test_w_expected("id = 1000 and generated gt -1m and level = 'error'", '%level%', 1))
		r.add(self.test_w_expected("id = 1000 and generated gt -1m and level = 'info'", '%level%', 1))
		result.add(r)
		
		r = TestResult('Checking syntax')
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%source% - %type% - %category%', 'Application Error - error - 0'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%source% - %type% - %category%', 'Application Error - info - 1'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%facility% - %qualifier% - %customer%', '0 - 0 - 0'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%facility% - %qualifier% - %customer%', '5 - 5 - 0'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%rawid% - %severity% - %log%', '1000 - success - Application'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%rawid% - %severity% - %log%', '2147812328 - warning - Application'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%id% - %strings%', '1000 - a, a, a, a, a, a, a, a, a, a, a, a, a, '))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%id% - %strings%', '1000 - a, a, a, a, a, a, a, a, a, a, a, a, a, '))
		result.add(r)

		return result

	def install(self, arguments):
		conf = Settings.get()
		conf.set_string('/modules', 'pytest_eventlog', 'CheckEventLog')
		conf.set_string('/modules', 'pytest', 'PythonScript')

		conf.set_string('/settings/pytest/scripts', 'test_eventlog', 'test_eventlog.py')
		
		conf.set_string('/settings/pytest_eventlog/real-time', 'enabled', 'true')
		conf.set_string('/settings/pytest_eventlog/real-time', 'filter', 'id = 1000 and category = 0')
		conf.set_string('/settings/pytest_eventlog/real-time/filters', 'test', 'id = 1000 and category = 1')
		conf.set_string('/settings/pytest_eventlog/real-time', 'maximum age', '5s')
		conf.set_string('/settings/pytest_eventlog/real-time', 'destination', 'pytest_evlog')
		conf.set_string('/settings/pytest_eventlog/real-time', 'language', 'english')
		conf.set_string('/settings/pytest_eventlog/real-time', 'debug', 'true')
		conf.set_string('/settings/pytest_eventlog/real-time', 'enable active', 'true')
		
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
