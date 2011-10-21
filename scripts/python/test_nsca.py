from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import Callable, TestResult, get_test_manager, create_test_manager
import plugin_pb2
from types import *
import socket
import uuid
import unicodedata

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

class NSCAMessage:
	uuid = None
	source = None
	command = None
	status = None
	message = None
	perfdata = None
	got_simple_response = False

	def __init__(self, command):
		try:
			self.uuid = command.decode('ascii')
		except UnicodeDecodeError:
			self.uuid = command
		#self.uuid = unicodedata.normalize('NFKD', command).encode('ascii','ignore')
		self.command = command
	def __str__(self):
		return 'Message: %s (%s, %s, %s)'%(self.uuid, self.source, self.command, self.status)

class NSCAServerTest:
	instance = None
	key = ''
	reg = None
	responses = {}
	
	class SingletonHelper:
		def __call__( self, *args, **kw ) :
			if NSCAServerTest.instance is None :
				object = NSCAServerTest()
				NSCAServerTest.instance = object
			return NSCAServerTest.instance

	getInstance = SingletonHelper()

	def desc(self):
		return 'Testcase for NSCA protocol'

	def title(self):
		return 'NSCA Server test'

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
		msg = NSCAMessage(command)
		if msg.uuid in self.responses:
			msg = self.responses[msg.uuid]
		msg.source = source
		msg.status = status
		msg.message = message
		msg.perfdata = perf
		msg.got_simple_response = True
		self.responses[msg.uuid] = msg
		return True

	def inbox_handler_wrapped(self, channel, request):
		log_error('DISCARDED message on %s'%(channel))
		
		message = plugin_pb2.SubmitRequestMessage()
		message.ParseFromString(request)
		command = message.payload[0].command
		log('Got simple message %s on %s'%(command, channel))
		
		msg = NSCAMessage(command)
		if msg.uuid in self.responses:
			msg = self.responses[msg.uuid]
		msg.got_response = True
		self.responses[msg.uuid] = msg
		return (False, '')
		
	def teardown(self):
		None
		
	def submit_payload(self, encryption, source, status, msg, perf):
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

		uid = str(uuid.uuid4())
		payload = message.payload.add()
		payload.result = status
		payload.command = uid
		payload.message = '%s - %s'%(uid, msg)
		payload.source = source
		(result_code, err) = core.submit('nsca_test_outbox', message.SerializeToString())
		result = TestResult()
		
		found = False
		for i in range(0,10):
			if uid in self.responses:
				rmsg = self.responses[uid]
				result.add_message(rmsg.got_response, 'Testing to recieve message using %s'%encryption)
				result.add_message(rmsg.got_simple_response, 'Testing to recieve simple message using %s'%encryption)
				result.add_message(len(err) == 0, 'Testing to send message using %s'%encryption, err)
				#result.assert_equals(rmsg.last_source, source, 'Verify that source is sent through')
				result.assert_equals(rmsg.command, uid, 'Verify that command is sent through')
				result.assert_contains(rmsg.message, msg, 'Verify that message is sent through')
				result.assert_equals(rmsg.perfdata, perf, 'Verify that performance data is sent through')
				del self.responses[uid]
				found = True
				break
			else:
				log('Waiting for %s (%s)'%(uid, self.responses.keys()))
				sleep(1)
		if not found:
			result.add_message(False, 'Failed to send message with uuid: %s using %s'%(uid, encryption), err)
		return result

	def test_one_full(self, encryption, state, key):
		return self.submit_payload(encryption, '%ssrc%s'%(key, key), state, '%smsg%s'%(key, key), '')

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
		# Currently broken: "xor"
		cryptos = ["des", "3des", "cast128", "xtea", "blowfish", "twofish", "rc2", "aes", "serpent", "gost", "none", "3way"]
		for c in cryptos:
			result.add_message(True, 'Testing crypto: %s'%c)
			result.add(self.test_one(c))
		
		return result
		
	def install(self, arguments):
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

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		None

	def shutdown(self):
		None

all_tests = [NSCAServerTest]

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
