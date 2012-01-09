from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
import plugin_pb2
from types import *
import socket
import uuid
import unicodedata
#import _thread
#sync = _thread.allocate_lock()

import threading
sync = threading.RLock()

core = Core.get()

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
		if type(command) == 'unicode':
			try:
				self.uuid = command.decode('ascii', 'replace')
			except UnicodeDecodeError:
				self.uuid = command
		else:
			self.uuid = command
		#self.uuid = unicodedata.normalize('NFKD', command).encode('ascii','ignore')
		self.command = command
	def __str__(self):
		return 'Message: %s (%s, %s, %s)'%(self.uuid, self.source, self.command, self.status)

class NSCAServerTest(BasicTest):
	instance = None
	key = ''
	reg = None
	_responses = {}
	
	def has_response(self, id):
		with sync:
			return id in self._responses
	
	def get_response(self, id):
		with sync:
			if id in self._responses:
				return self._responses[id]
			msg = NSCAMessage(id)
			self._responses[id] = msg
			return msg

	def set_response(self, msg):
		with sync:
			self._responses[msg.uuid] = msg

	def del_response(self, id):
		with sync:
			del self._responses[id]
			
	
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
		log('Got simple message %s'%command)
		msg = self.get_response(command)
		msg.source = source
		msg.status = status
		msg.message = message
		msg.perfdata = perf
		msg.got_simple_response = True
		self.set_response(msg)
		return True

	def inbox_handler_wrapped(self, channel, request):
		log_error('DISCARDING message on %s'%(channel))
		
		message = plugin_pb2.SubmitRequestMessage()
		message.ParseFromString(request)
		command = message.payload[0].command
		log('Got message %s on %s'%(command, channel))
		
		msg = self.get_response(command)
		msg.got_response = True
		self.set_response(msg)
		return None
		
	def teardown(self):
		None
		
	def wait_and_validate(self, uuid, result, msg, perf, tag):
		found = False
		for i in range(0,10):
			if self.has_response(uuid):
				rmsg = self.get_response(uuid)
				result.add_message(rmsg.got_response, 'Testing to recieve message using %s'%tag)
				result.add_message(rmsg.got_simple_response, 'Testing to recieve simple message using %s'%tag)
				#result.assert_equals(rmsg.last_source, source, 'Verify that source is sent through')
				result.assert_equals(rmsg.command, uuid, 'Verify that command is sent through using %s'%tag)
				result.assert_contains(rmsg.message, msg, 'Verify that message is sent through using %s'%tag)
				result.assert_equals(rmsg.perfdata, perf, 'Verify that performance data is sent through using %s'%tag)
				self.del_response(uuid)
				return True
			else:
				log('Waiting for %s (%d/10)'%(uuid, i+1))
				sleep(1)
		result.add_message(False, 'Failed to recieve message %s using %s'%(uuid, tag))
		return False
	
		
	def submit_payload(self, encryption, source, status, msg, perf, tag, retry=True):
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
		result.add_message(len(err) == 0, 'Testing to send message using %s/sbp'%tag, err)
		found = self.wait_and_validate(uid, result, msg, perf, '%s/spb'%tag)
		if retry and not found:
			return self.submit_payload(encryption, source, status, msg, perf, tag, False)
		return result
		
	def submit_via_exec(self, encryption, source, status, msg, perf, tag, retry=True):
		uid = str(uuid.uuid4())
	
		args = [
			#'--exec', 'submit', 
			'--alias', uid, 
			'--result', '%d'%status, 
			'--message', '%s - %s'%(uid, msg), 
			'--encryption', encryption,
			'--password', 'pwd-%s'%encryption,
			'--host', '127.0.0.1:15667'
			]
		(result_code, result_message) = core.simple_exec('any', 'nsca_submit', args)
		result = TestResult()
		
		result.add_message(result_code == 0, 'Testing to send message using %s/exec:1'%tag)
		result.add_message(len(result_message) == 1, 'Testing to send message using %s/exec:2'%tag, len(result_message))
		result.add_message(len(result_message[0]) == 0, 'Testing to send message using %s/exec:3'%tag, result_message[0])
		found = self.wait_and_validate(uid, result, msg, perf, '%s/exec'%tag)
		if retry and not found:
			return self.submit_via_exec(encryption, source, status, msg, perf, tag, False)
		return result

	def test_one_crypto_full(self, encryption, state, key):
		result = TestResult()
		result.add(self.submit_payload(encryption, '%ssrc%s'%(key, key), state, '%smsg%s'%(key, key), '', '%s/%s'%(state, encryption)))
		result.add(self.submit_via_exec(encryption, '%ssrc%s'%(key, key), state, '%smsg%s'%(key, key), '', '%s/%s'%(state, encryption)))
		return result

	def test_one_crypto(self, crypto):
		conf = Settings.get()
		conf.set_string('/settings/NSCA/test_nsca_server', 'encryption', '%s'%crypto)
		conf.set_string('/settings/NSCA/test_nsca_server', 'password', 'pwd-%s'%crypto)
		core.reload('test_nsca_server')
		result = TestResult()
		result.add_message(isOpen('localhost', 15667), 'Checking that port is open')
		result.add(self.test_one_crypto_full(crypto, status.UNKNOWN, 'unknown'))
		result.add(self.test_one_crypto_full(crypto, status.OK, 'ok'))
		result.add(self.test_one_crypto_full(crypto, status.WARNING, 'warn'))
		result.add(self.test_one_crypto_full(crypto, status.CRITICAL, 'crit'))
		return result

	def run_test(self):
		result = TestResult()
		# Currently broken: "xor"
		cryptos = ["des", "3des", "cast128", "xtea", "blowfish", "twofish", "rc2", "aes", "serpent", "gost", "none", "3way"]
		for c in cryptos:
			result.add_message(True, 'Testing crypto: %s'%c)
			result.add(self.test_one_crypto(c))
		
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

setup_singleton(NSCAServerTest)

all_tests = [NSCAServerTest]

def __main__():
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
