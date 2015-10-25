from NSCP import Settings, Registry, Core, log, status, log_error, log_debug, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
import plugin_pb2
from types import *
import socket
import uuid
import unicodedata

import threading
sync = threading.RLock()

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
	got_response = False
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
		self.source = None
		self.status = None
		self.message = None
		self.perfdata = None
		self.got_response = False
		self.got_simple_response = False
		
		
	def copy_changed_attributes(self, other):
		if other.source:
			self.source = other.source
		if other.status:
			self.status = other.status
		if other.message:
			self.message = other.message
		if other.perfdata:
			self.perfdata = other.perfdata
		if other.got_response:
			self.got_response = True
		if other.got_simple_response:
			self.got_simple_response = True
	
	def __str__(self):
		return 'Message: %s (%s, %s, %s)'%(self.uuid, self.source, self.command, self.status)

class NSCAServerTest(BasicTest):
	instance = None
	key = ''
	reg = None
	conf = None
	core = None
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
			if msg.uuid in self._responses:
				self._responses[msg.uuid].copy_changed_attributes(msg)
			else:
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
		log_debug('Got message %s on %s'%(command, channel))
		msg = NSCAMessage(command)
		msg.source = source
		msg.status = status
		msg.message = message
		msg.perfdata = perf
		msg.got_simple_response = True
		self.set_response(msg)
		return True

	def inbox_handler_wrapped(self, channel, request):
		message = plugin_pb2.SubmitRequestMessage()
		message.ParseFromString(request)
		if len(message.payload) != 1:
			log_error("Got invalid message on channel: %s"%channel)
			return None
		command = message.payload[0].command
		log_debug('Got message %s on %s'%(command, channel))
		
		msg = NSCAMessage(command)
		msg.got_response = True
		self.set_response(msg)
		return None
		
	def teardown(self):
		None
		
	def wait_and_validate(self, uuid, result, msg, perf, tag):
		found = False
		for i in range(0,10):
			if not self.has_response(uuid):
				log_debug('Waiting for %s (%d/10)'%(uuid, i+1))
				sleep(200)
			else:
				log_debug('Got response %s'%uuid)
				found = True
				break
		if not found:
			result.add_message(False, 'Failed to recieve message %s using %s'%(uuid, tag))
			return False
		
		for i in range(0,10):
			rmsg = self.get_response(uuid)
			if not rmsg.got_simple_response or not rmsg.got_response:
				log_debug('Waiting for delayed response %s s/m: %s/%s - (%d/10)'%(uuid, rmsg.got_simple_response, rmsg.got_response, i+1))
				sleep(500)
			else:
				log_debug('Got delayed response %s'%uuid)
				break
		
		result.add_message(rmsg.got_response, 'Testing to recieve message using %s'%tag)
		result.add_message(rmsg.got_simple_response, 'Testing to recieve simple message using %s'%tag)
		result.assert_equals(rmsg.command, uuid, 'Verify that command is sent through using %s'%tag)
		result.assert_contains(rmsg.message, msg, 'Verify that message is sent through using %s'%tag)
		
		#result.assert_equals(rmsg.last_source, source, 'Verify that source is sent through')
		#result.assert_equals(rmsg.perfdata, perf, 'Verify that performance data is sent through using %s'%tag)
		self.del_response(uuid)
		return True

	def submit_payload(self, encryption, target, length, source, status, msg, perf, tag):
		message = plugin_pb2.SubmitRequestMessage()

		message.header.recipient_id = target
		message.channel = 'nsca_test_outbox'
		host = message.header.hosts.add()
		host.id = target
		if (target == 'valid'):
			pass
		else:
			host.address = "127.0.0.1:15667"
			enc = host.metadata.add()
			enc.key = "encryption"
			enc.value = encryption
			enc = host.metadata.add()
			enc.key = "password"
			enc.value = 'pwd-%s'%encryption
			enc = host.metadata.add()
			enc.key = "payload length"
			enc.value = '%d'%length

		uid = str(uuid.uuid4())
		payload = message.payload.add()
		payload.result = status
		payload.command = uid
		line = payload.lines.add()
		line.message = '%s - %s'%(uid, msg)
		payload.source = source
		(result_code, err) = self.core.submit('nsca_test_outbox', message.SerializeToString())

		result = TestResult('Testing payload submission (via API): %s'%tag)
		result.assert_equals(result_code, True, 'Submission (%s) return ok status'%tag)
		result.assert_equals(err, 'Submission successful', 'Submission (%s) returned correct status'%tag)
		self.wait_and_validate(uid, result, msg, perf, '%s/spb'%tag)
		return result

		
	def submit_via_exec(self, encryption, target, length, source, status, msg, perf, tag):
		uid = str(uuid.uuid4())
	
		args = [
			#'--exec', 'submit', 
			'--alias', uid, 
			'--result', '%d'%status, 
			'--retries', '0',
			'--message', '%s - %s'%(uid, msg), 
			'--target', target,
			]
		if (target == 'valid'):
			pass
		else:
			args.extend([
				'--address', '127.0.0.1:15667',
				'--encryption', encryption,
				'--password', 'pwd-%s'%encryption,
				'--payload-length', '%d'%length,
			])
		(result_code, result_message) = self.core.simple_exec('test_nsca_client', 'nsca_submit', args)
		result = TestResult('Testing payload submission (via command line exec): %s'%tag)
		
		result.add_message(result_code == 0, 'Testing to send message using %s/exec:1'%tag)
		result.add_message(len(result_message) == 1, 'Testing to send message using %s/exec:2'%tag)
		if len(result_message) == 1:
			result.assert_equals(result_message[0], "Submission successful", 'Testing to send message using %s/exec:3'%tag)
		self.wait_and_validate(uid, result, msg, perf, '%s/exec'%tag)
		return result

	def test_one_crypto_full(self, encryption, state, key, target, length):
		result = TestResult('Testing %s/%s'%(encryption, key))
		result.add(self.submit_payload(encryption, target, length, '%ssrc%s'%(key, key), state, '%smsg%s'%(key, key), '', '%s/%s/%d/%s'%(state, encryption, length, target)))
		result.add(self.submit_via_exec(encryption, target, length, '%ssrc%s'%(key, key), state, '%smsg%s'%(key, key), '', '%s/%s/%d/%s'%(state, encryption, length, target)))
		return result

	def test_one_crypto(self, crypto, length=512):
		log('Testing: %s %d'%(crypto, length))
		conf = self.conf
		conf.set_string('/settings/NSCA/test_nsca_server', 'encryption', '%s'%crypto)
		conf.set_string('/settings/NSCA/test_nsca_server', 'password', 'pwd-%s'%crypto)
		conf.set_int('/settings/NSCA/test_nsca_server', 'payload length', length)
		self.core.reload('test_nsca_server')
		
		conf.set_string('/settings/NSCA/test_nsca_client/targets/default', 'address', 'nsca://127.0.0.1:35667')
		conf.set_string('/settings/NSCA/test_nsca_client/targets/default', 'encryption', '%s'%crypto)
		conf.set_string('/settings/NSCA/test_nsca_client/targets/default', 'password', 'default-%s'%crypto)
		conf.set_int('/settings/NSCA/test_nsca_client/targets/default', 'payload length', length*3)

		conf.set_string('/settings/NSCA/test_nsca_client/targets/invalid', 'address', 'nsca://127.0.0.1:25667')
		conf.set_string('/settings/NSCA/test_nsca_client/targets/invalid', 'encryption', 'none')
		conf.set_string('/settings/NSCA/test_nsca_client/targets/invalid', 'password', 'invalid-%s'%crypto)
		conf.set_int('/settings/NSCA/test_nsca_client/targets/invalid', 'payload length', length*2)

		conf.set_string('/settings/NSCA/test_nsca_client/targets/valid', 'address', 'nsca://127.0.0.1:15667')
		conf.set_string('/settings/NSCA/test_nsca_client/targets/valid', 'encryption', '%s'%crypto)
		conf.set_string('/settings/NSCA/test_nsca_client/targets/valid', 'password', 'pwd-%s'%crypto)
		conf.set_int('/settings/NSCA/test_nsca_client/targets/valid', 'payload length', length)
		self.core.reload('test_nsca_client')

		
		
		result = TestResult('Testing: %s/%d'%(crypto, length))
		result.add_message(isOpen('localhost', 15667), 'Checking that port is open')
		for target in ['valid', 'test_rp', 'invalid']:
			result.add(self.test_one_crypto_full(crypto, status.UNKNOWN, 'unknown', target, length))
			result.add(self.test_one_crypto_full(crypto, status.OK, 'ok', target, length))
			result.add(self.test_one_crypto_full(crypto, status.WARNING, 'warn', target, length))
			result.add(self.test_one_crypto_full(crypto, status.CRITICAL, 'crit', target, length))
		return result

	def run_test(self, cases=None):
		result = TestResult()
		cryptos = ["none", "xor", "des", "3des", "cast128", "xtea", "blowfish", "twofish", "rc2", "aes", "aes256", "aes192", "aes128", "serpent", "gost", "3way"]
		for c in cryptos:
			run_l = None
			run_this = False
			if cases:
				tmp_l = None
				for case in cases:
					if '-' in case:
						(run_c, tmp_l) = case.split('-', 2)
					else:
						run_c = case
					if c == run_c:
						run_l = int(tmp_l) if tmp_l else None
						run_this = True
				if not run_this:
					result.add_message(True, 'Ignoring: %s-*'%c)
					continue
			for l in [128, 512, 1024, 4096]:
				if not run_l or run_l == l:
					result.add(self.test_one_crypto(c, l))
				else:
					result.add_message(True, 'Ignoring: %s-%s'%(c, l))
		
		return result
		
	def install(self, arguments):
		conf = self.conf
		conf.set_string('/modules', 'test_nsca_server', 'NSCAServer')
		conf.set_string('/modules', 'test_nsca_client', 'NSCAClient')
		conf.set_string('/modules', 'pytest', 'PythonScript')

		conf.set_string('/settings/pytest/scripts', 'test_nsca', 'test_nsca.py')
		
		conf.set_string('/settings/NSCA/test_nsca_server', 'port', '15667')
		conf.set_string('/settings/NSCA/test_nsca_server', 'inbox', 'nsca_test_inbox')
		conf.set_string('/settings/NSCA/test_nsca_server', 'encryption', '1')

		conf.set_string('/settings/NSCA/test_nsca_client', 'channel', 'nsca_test_outbox')
		
		conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def shutdown(self):
		None
		
	def require_boot(self):
		return True
		

setup_singleton(NSCAServerTest)

all_tests = [NSCAServerTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
