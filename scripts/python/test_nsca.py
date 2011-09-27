from NSCP import Settings, Registry, Core, log, status
from test_helper import Callable, run_tests, TestResult
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
	last_source = None
	last_command = None
	last_status = None
	last_message = None
	last_perfdata = None
	got_simple_response = None
	
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
		self.reg.simple_subscription('nsca_test_inbox', NSCAServerTest.simple_inbox_handler)
		self.reg.subscription('nsca_test_inbox', NSCAServerTest.inbox_handler)

	def simple_inbox_handler(channel, source, command, code, message, perf):
		instance = NSCAServerTest.getInstance()
		return instance.simple_inbox_handler_wrapped(channel, source, command, code, message, perf)
	simple_inbox_handler = Callable(simple_inbox_handler)

	def inbox_handler(channel, request):
		instance = NSCAServerTest.getInstance()
		return instance.inbox_handler_wrapped(channel, request)
	inbox_handler = Callable(inbox_handler)
	
	def simple_inbox_handler_wrapped(self, channel, source, command, status, message, perf):
		log('Got simple message %s on %s'%(command, channel))
		self.got_simple_response = True
		self.last_source = source
		self.last_command = command
		self.last_status = status
		self.last_message = message
		self.last_perfdata = perf
		return True

	def inbox_handler_wrapped(self, channel, request):
		self.got_response = True
		return (False, '')
		
	def teardown(self):
		None
		
	def submit_payload(self, encryption, source, command, status, msg, perf):
		message = plugin_pb2.SubmitRequestMessage()
		
		message.header.version = plugin_pb2.Common.VERSION_1
		message.header.recipient_id = "test_rp"
		message.channel = 'nsca_test_outbox'
		host = message.header.hosts.add()
		host.address = "127.0.0.1:15667"
		host.id = "test_rp"
		enc = host.metadata.add()
		enc.key = "encryption"
		enc.value = encryption
		enc = host.metadata.add()
		enc.key = "password"
		enc.value = 'pwd-%s'%encryption

		payload = message.payload.add()
		payload.result = status
		payload.command = command
		payload.message = msg
		payload.source = source
		self.got_response = False
		self.got_simple_response = False
		(result_code, err) = core.submit('nsca_test_outbox', message.SerializeToString())
		result = TestResult()
		result.add_message(len(err) == 0, 'Testing to send message using %s'%encryption, err)
		result.add_message(self.got_simple_response, 'Testing to recieve simple message using %s'%encryption)
		#result.assert_equals(self.last_source, source, 'Verify that source is sent through')
		result.assert_equals(self.last_command, command, 'Verify that command is sent through')
		result.assert_equals(self.last_message, msg, 'Verify that message is sent through')
		result.assert_equals(self.last_perfdata, perf, 'Verify that performance data is sent through')
		result.add_message(self.got_response, 'Testing to recieve message')
		return result

	def test_one_full(self, encryption, state, key):
		return self.submit_payload(encryption, '%ssrc%s'%(key, key), '%scmd%s'%(key, key), state, '%smsg%s'%(key, key), '')

	def test_one(self, crypto):
		conf = Settings.get()
		conf.set_string('/settings/NSCA/test_nsca_server', 'encryption', '%s'%crypto)
		conf.set_string('/settings/NSCA/test_nsca_server', 'password', 'pwd-%s'%crypto)
		core.reload('test_nsca_server')
		result = TestResult()
		result.add(self.test_one_full(crypto, status.UNKNOWN, 'unknown'))
		result.add(self.test_one_full(crypto, status.OK, 'ok'))
		result.add(self.test_one_full(crypto, status.WARNING, 'warn'))
		result.add(self.test_one_full(crypto, status.CRITICAL, 'crit'))
		return result

	def run_test(self):
		result = TestResult()
		result.add_message(isOpen('localhost', 15667), 'Checking that port is open')
		# seems broken: "xor", "3way"
		for c in ["des", "cast128", "xtea", "blowfish", "twofish", "rc2", "aes", "serpent", "gost", "none"]:
			result.add(self.test_one(c))
		
		return result

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
	log(' | Then start the test_nsca command by typing it and press enter like so:   |')
	log(' | test_nsca                                                                |')
	log(' | Lastly exit by typing exit like so:                                      |')
	log(' | exit                                                                     |')
	log('-+--------------------------------------------------------==(DAS ENDE!)==---+-')
	conf = Settings.get()
	conf.set_string('/modules', 'test_nsca_server', 'NSCAServer')
	conf.set_string('/modules', 'test_nsca_client', 'NSCAClient')
	conf.set_string('/modules', 'pytest', 'PythonScript')

	conf.set_string('/settings/pytest/scripts', 'test_nsca', 'test_nsca.py')
	
	conf.set_string('/settings/NSCA/test_nsca_server', 'port', '15667')
	conf.set_string('/settings/NSCA/test_nsca_server', 'inbox', 'nsca_test_inbox')
	conf.set_string('/settings/NSCA/test_nsca_server', 'encryption', '1')

	conf.set_string('/settings/NSCA/test_nsca_client/targets', 'nsca_test_local', 'nsca://127.0.0.1:15667')
	conf.set_string('/settings/NSCA/test_nsca_client', 'channel', 'nsca_test_outbox')
	
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
