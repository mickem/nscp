from NSCP import Settings, Registry, Core, log, status
from test_helper import Callable, run_tests, log_test_result, log_result, TestResult
import plugin_pb2
from types import *
import socket

core = Core.get()

prefix = 'py_'
plugin_id = 0

def get_help(arguments):
	return (status.OK, 'help: Get help')


def isOpen(ip, port):
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	try:
		s.connect((ip, int(port)))
		s.shutdown(2)
		return True
	except:
		return False

class NSCAServerTest:
	instance = None
	key = ''
	reg = None
	got_response = False
	
	class SingletonHelper:
		def __call__( self, *args, **kw ) :
			if NSCAServerTest.instance is None :
				object = NSCAServerTest()
				NSCAServerTest.instance = object
			return NSCAServerTest.instance

	getInstance = SingletonHelper()

	def desc(self):
		return 'Testing that channels are loaded'

	def setup(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)
		self.reg.simple_subscription('nsca_test_inbox', NSCAServerTest.inbox_handler)

	def inbox_handler(channel, command, code, message, perf):
		instance = NSCAServerTest.getInstance()
		instance.inbox_handler_wrapped(channel, command, code, message, perf)
	inbox_handler = Callable(inbox_handler)
	
	def inbox_handler_wrapped(self, channel, command, code, message, perf):
		log('Got message %s on %s'%(command, channel))
		self.got_response = True
		
	def teardown(self):
		None
		
	def submit_payload(self):
		message = plugin_pb2.SubmitRequestMessage()
		
		message.header.version = plugin_pb2.Common.VERSION_1
		message.header.recipient_id = "test_rp"
		message.channel = 'TESTNSCA'
		host = message.header.hosts.add()
		host.address = "127.0.0.1:15667"
		host.id = "test_rp"

		payload = message.payload.add()
		payload.result = status.UNKNOWN
		payload.command = 'hello_world'
		payload.message = 'test'
		self.got_response = False
		(state, result) = core.submit('TESTNSCA', message.SerializeToString())
		log("Status: %s"%status)
		return self.got_response

	def run_test(self):
		result = TestResult()
		result.add_message(isOpen('localhost', 15667), 'Checking that port is open')
		result.add_message(self.submit_payload(), 'Submitting payload to target')
		
		return result.log()

def test(arguments):
	global prefix
	global plugin_id
	result = TestResult()
	result.add(run_tests(plugin_id, prefix, [NSCAServerTest]))
	return result.return_nagios()

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
	conf.set_string('/modules', 'test_nsca_server', 'NSCAServer')
	conf.set_string('/modules', 'test_nsca_client', 'NSCAClient')
	conf.set_string('/settings/pytest/scripts', 'test_nsca', 'test_nsca.py')
	
	conf.set_string('/settings/NSCA/test_nsca_server', 'port', '15667')
	conf.set_string('/settings/NSCA/test_nsca_server', 'inbox', 'nsca_test_inbox')

	conf.set_string('/settings/NSCA/test_nsca_client/targets', 'nsca_test_local', 'nsca://127.0.0.1:15667')
	
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

	reg = Registry.get(plugin_id)
	
	reg.simple_cmdline('help', get_help)
	reg.simple_cmdline('install_python_test', install_test)

	reg.simple_function('test_nsca', test, 'Run python NSCA unit test suite')

def shutdown():
	log('Unloading script...')
