from NSCP import Settings, Registry, Core, log, status, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
import plugin_pb2
from types import *
import socket
import unicodedata
import uuid


class Message:
	uuid = None
	channel = None
	source = None
	command = None
	status = None
	message = None
	perf = None
	tag = None
	delivered = False

	def __init__(self, channel = None, source = None, command = None, status = None, message = None, perf = None, tag = None):
		if not channel:
			self.uuid = str(uuid.uuid4())
		self.channel = channel
		self.source = source
		self.command = command
		self.status = status
		self.message = message
		self.perf = perf
		self.delivered = False
		self.tag = tag

	def copy_from(self, other):
		self.uuid = other.uuid
		self.channel = other.channel
		self.source = other.source
		self.command = other.command
		self.status = other.status
		self.message = other.message
		self.perf = other.perf
		self.delivered = other.delivered
		self.tag = other.tag

	def __str__(self):
		return 'Message: %s (%s, %s, %s)'%(self.uuid, self.channel, self.status, self.message)
	
	def __repr__(self):
		return self.__str__()


class EventLogTest(BasicTest):
	instance = None
	key = ''
	reg = None
	conf = None
	core = None
	last_tag = []
	got_simple_response = None
	message_count = 0
	messages = []
	
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
		self.reg.simple_subscription('pytest_evlog_01', EventLogTest.simple_inbox_handler_01)
		self.reg.simple_subscription('pytest_evlog_02', EventLogTest.simple_inbox_handler_02)

	def simple_inbox_handler_01(channel, source, command, code, message, perf):
		instance = EventLogTest.getInstance()
		return instance.simple_inbox_handler_wrapped(channel, source, command, code, message, perf, '001')
	simple_inbox_handler_01 = Callable(simple_inbox_handler_01)

	def simple_inbox_handler_02(channel, source, command, code, message, perf):
		instance = EventLogTest.getInstance()
		return instance.simple_inbox_handler_wrapped(channel, source, command, code, message, perf, '002')
	simple_inbox_handler_02 = Callable(simple_inbox_handler_02)
	
	def simple_inbox_handler_wrapped(self, channel, source, command, status, message, perf, tag):
		msg = Message(channel, source, command, status, message, perf, tag)
		msg.delivered = True
		self.messages.append(msg)
		log('Recieved: %s'%msg)
		return True

	def teardown(self):
		None

	def test_create(self, source, id, level, severity, category, facility, arguments):
		result = TestResult('Creating log message: i:%d, l:%s, s:%s, c:%d, f:%d'%(id, level, severity, category, facility))
		args = ['--source', source, 
					'--id', id,				# Any number (corresponds with message identifier)								-- Identifies message
					'--level', level,		# error(1), warning(2), success(0), info(4), auditSuccess(8), auditFailure(10)	-- Loglevel severity (ie log level)
					'--severity', severity,	# success(0), informational(1), warning(2), error(3) 							-- Developer severity (ie classification)
					'--category', category,	#
					'--facility', facility	#
					]
		for f in arguments:
			args.append('--argument')
			args.append(f)
		(ret, msg) = self.core.simple_exec('eventlog', 'insert', args)
		result.assert_equals(ret, 0, 'return code')
		result.assert_equals(len(msg), 1, 'Message length')
		if len(msg) == 1:
			result.assert_equals(msg[0], 'Message reported successfully', 'Status message')
		return result
		
		
	def test_w_expected(self, filter, syntax, expected):
		result = TestResult('Validating filter: %s (%d)'%(filter, expected))
		(res, msg, perf) = self.core.simple_query('CheckEventLog', ['file=Application', 'debug=false', 'warn=gt:%d'%expected, 'crit=gt:%d'%expected, 'filter=%s'%filter, 'syntax=%s'%syntax, 'scan-range=-10m', 'top-syntax=${status} ${count}==%d: ${list}'%expected])
		result.assert_equals(res, status.OK, "Validate status OK for %s"%filter)
		(res, msg, perf) = self.core.simple_query('CheckEventLog', ['file=Application', 'debug=false', 'warn=eq:%d'%expected, 'crit=gt:%d'%expected, 'filter=%s'%filter, 'syntax=%s'%syntax, 'scan-range=-10m', 'top-syntax=${status} ${count}==%d: ${list}'%expected])
		result.assert_equals(res, status.WARNING, "Validate status OK for %s"%filter)
		(res, msg, perf) = self.core.simple_query('CheckEventLog', ['file=Application', 'debug=false', 'warn=eq:%d'%expected, 'crit=eq:%d'%expected, 'filter=%s'%filter, 'syntax=%s'%syntax, 'scan-range=-10m', 'top-syntax=${status} ${count}==%d: ${list}'%expected])
		result.assert_equals(res, status.CRITICAL, "Validate status CRIT for %s"%filter)
		return result

	def test_syntax(self, filter, syntax, expected):
		result = TestResult('Validating syntax: %s'%syntax)
		(res, msg, perf) = self.core.simple_query('CheckEventLog', ['file=Application', 'warn=ne:1', 'filter=%s'%filter, 'syntax=%s'%syntax, 'descriptions', 'scan-range=-10m'])
		result.assert_equals(msg, expected, "Validate message rendering syntax: %s"%msg)
		return result
		
	def run_test(self):
		result = TestResult('Checking CheckEventLog')
		cache = TestResult('Checking CheckEventLog CACHE')

		sleep(2000)
		
		#(res, msg, perf) = self.core.simple_query('CheckEventLogCACHE', ['warn=eq:1', 'crit=eq:2'])
		#cache.assert_equals(res, status.OK, "Validate cache is empty")
		#cache.assert_equals(msg, 'Eventlog check ok', "Validate cache is ok: %s"%msg)
		
		
		a_list = ['a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a']
		result.add(self.test_create('Application Error', 1000, 'error', 	'success', 			0, 0, a_list))
		result.add(self.test_create('Application Error', 1000, 'warning',  	'informational', 	1, 5, a_list))
		result.add(self.test_create('Application Error', 1000, 'success',  	'warning', 			2, 5, a_list))
		result.add(self.test_create('Application Error', 1000, 'info',		'error', 			3, 5, a_list))
		for x in range(1,10):
			log('Waiting...%d/4.'%len(self.messages))
			sleep(100)
			if len(self.messages) == 4:
				break
		log('Recieved %d messages.'%len(self.messages))
		result.assert_equals(len(self.messages), 4, 'Verify that all 4 messages are sent through')
		
		for msg in self.messages:
			if msg.message.startswith('X1'):
				r = TestResult('Validating message X1')
				r.assert_equals(msg.message, 'X1 warning Application Error: ', 'Verify message')
				r.assert_equals(msg.channel, 'pytest_evlog_01', 'Verify channel')
				r.assert_equals(msg.tag, '001', 'Verify tag')
				r.assert_equals(msg.status, status.WARNING, 'Verify status')
				result.add(r)
			elif msg.message.startswith('X2'):
				r = TestResult('Validating message X2')
				r.assert_equals(msg.message, 'X2 success Application Error: ', 'Verify message')
				r.assert_equals(msg.channel, 'pytest_evlog_02', 'Verify channel')
				r.assert_equals(msg.tag, '002', 'Verify tag')
				r.assert_equals(msg.status, status.CRITICAL, 'Verify status')
				result.add(r)
			elif msg.message.startswith('X3'):
				r = TestResult('Validating message X3')
				r.assert_equals(msg.message, 'X3 info Application Error: ', 'Verify message')
				r.assert_equals(msg.channel, 'pytest_evlog_01', 'Verify channel')
				r.assert_equals(msg.tag, '001', 'Verify tag')
				r.assert_equals(msg.status, status.UNKNOWN, 'Verify status')
				result.add(r)
			elif msg.message.startswith('X4'):
				r = TestResult('Validating message X4')
				r.assert_equals(msg.message, 'X4 error Application Error: ', 'Verify message')
				r.assert_equals(msg.channel, 'pytest_evlog_01', 'Verify channel')
				r.assert_equals(msg.tag, '001', 'Verify tag')
				r.assert_equals(msg.status, status.OK, 'Verify status')
				result.add(r)
		
		#(res, msg, perf) = self.core.simple_query('CheckEventLogCACHE', ['warn=eq:1', 'crit=eq:4'])
		#cache.assert_equals(res, status.CRITICAL, "Validate cache has items: %s"%msg)
		#cache.assert_equals(msg, 'X4 error Application Error: , X1 warning Application Error: , X2 success Application Error: , X3 info Application Error: , eventlog: 4 = critical', "Validate cache message")
		#cache.assert_equals(perf, "'eventlog'=4;1;4", "Validate cache performance")
		#(res, msg, perf) = self.core.simple_query('CheckEventLogCACHE', ['warn=eq:1', 'crit=eq:2'])
		#cache.assert_equals(res, status.OK, "Validate cache is empty (again)")
		#cache.assert_equals(msg, 'Eventlog check ok', "Validate cache is ok: %s"%msg)
		
		#result.add(cache)
		
		r = TestResult('Checking filters')
		r.add(self.test_w_expected('id = 1000 and generated gt 1m', '%generated%', 0))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m', '%generated%', 4))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m and id = 1000', '%generated%: %id%, %category%', 4))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m and category = 1', '%category%', 1))
		r.add(self.test_w_expected('id = 1000 and generated gt -1m and category = 0', '%category%', 1))
		r.add(self.test_w_expected("id = 1000 and generated gt -1m and level = 'error'", '%level%', 1))
		r.add(self.test_w_expected("id = 1000 and generated gt -1m and level = 'warning'", '%level%', 1))
		result.add(r)
		
		r = TestResult('Checking syntax')
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%source% - %type% - %category%', 'Application Error - error - 0'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%source% - %type% - %category%', 'Application Error - warning - 1'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 2', '%source% - %type% - %category%', 'Application Error - information - 2'))
		r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 3', '%source% - %type% - %category%', 'Application Error - information - 3'))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%facility% - %qualifier% - %customer%', '0 - 0 - 0'))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%facility% - %qualifier% - %customer%', '5 - 5 - 0'))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%rawid% - %severity% - %log%', '1000 - success - Application'))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%rawid% - %severity% - %log%', '1074070504 - informational - Application'))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 2', '%rawid% - %severity% - %log%', '2147812328 - warning - Application'))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 3', '%rawid% - %severity% - %log%', '3221554152 - error - Application'))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 0', '%id% - %strings%', '1000 - a, a, a, a, a, a, a, a, a, a, a, a, a, '))
		#r.add(self.test_syntax('id = 1000 and generated gt -2m and category = 1', '%id% - %strings%', '1000 - a, a, a, a, a, a, a, a, a, a, a, a, a, '))
		result.add(r)

		return result

	def install_filter(self, conf, path, target, filter = None, syntax = '%type% %source%: %message%', severity = 'OK', lang = 'english', age = '5s'):
		if filter:
			conf.set_string(path, 'filter', filter)
		if target:
			conf.set_string(path, 'target', target)
		if lang:
			conf.set_string(path, 'language', lang)
		if age:
			conf.set_string(path, 'maximum age', age)
		if syntax:
			conf.set_string(path, 'detail syntax', syntax)
		if severity:
			conf.set_string(path, 'severity', severity)
		conf.set_string(path, 'log', 'application')
		conf.set_string(path, 'debug', 'true')
	
	def install(self, arguments):
		conf = self.conf
		conf.set_string('/modules', 'pytest_eventlog', 'CheckEventLog')
		conf.set_string('/modules', 'pytest', 'PythonScript')

		conf.set_string('/settings/pytest/scripts', 'test_eventlog', 'test_eventlog.py')
		
		conf.set_string('/settings/pytest_eventlog/real-time', 'enabled', 'true')

		self.install_filter(conf, '/settings/pytest_eventlog/real-time/filters/default', 'pytest_evlog_01', 'id = 1000 and category = 0', '%type% %source%: %message%','OK')
		base_syntax = '${id},${category} ${source}: ${message}'
		
		self.install_filter(conf, '/settings/pytest_eventlog/real-time/filters/py_test_001', 'pytest_evlog_01', 'id = 1000 and category = 1', 'X1 %s'%base_syntax, 'WARNING')
		self.install_filter(conf, '/settings/pytest_eventlog/real-time/filters/py_test_002', 'pytest_evlog_02', 'id = 1000 and category = 2', 'X2 %s'%base_syntax, 'CRITICAL')
		self.install_filter(conf, '/settings/pytest_eventlog/real-time/filters/py_test_003',  None, 			'id = 1000 and category = 3', 'X3 %s'%base_syntax, 'UNKNOWN')
		self.install_filter(conf, '/settings/pytest_eventlog/real-time/filters/py_test_004',  None, 			None                        , 'X4 %s'%base_syntax, None)
		
		conf.set_string('/settings/pytest_eventlog/real-time', 'maximum age', '5s')
		conf.set_string('/settings/pytest_eventlog/real-time', 'debug', 'true')
		conf.set_string('/settings/pytest_eventlog/real-time', 'enable active', 'true')
		
		conf.save()
	
	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def shutdown(self):
		None

setup_singleton(EventLogTest)

all_tests = [EventLogTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
