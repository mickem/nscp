from NSCP import Settings, Registry, Core, log, status, log_error, sleep
import sys

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

class NRPEMessage:
	uuid = None
	source = None
	command = None
	status = None
	message = None
	perfdata = None
	got_simple_response = False
	got_response = False

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

class NRPEServerTest(BasicTest):
	instance = None
	key = ''
	reg = None
	conf = None
	core = None
	_responses = {}
	_requests = {}
	
	def has_response(self, id):
		with sync:
			return id in self._responses
	
	def get_response(self, id):
		with sync:
			if id in self._responses:
				return self._responses[id]
			msg = NRPEMessage(id)
			self._responses[id] = msg
			return msg

	def set_response(self, msg):
		with sync:
			self._responses[msg.uuid] = msg

	def del_response(self, id):
		with sync:
			del self._responses[id]
			
	def get_request(self, id):
		with sync:
			if id in self._requests:
				return self._requests[id]
			msg = NRPEMessage(id)
			self._requests[id] = msg
			return msg

	def set_request(self, msg):
		msg.got_simple_response = False
		msg.got_response = False
		with sync:
			self._requests[msg.uuid] = msg

	def del_request(self, id):
		with sync:
			del self._requests[id]
			
	
	def desc(self):
		return 'Testcase for NRPE protocol'

	def title(self):
		return 'NRPE Client/Server test'

	def init(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def setup(self, plugin_id, prefix):
		self.reg.simple_function('check_py_nrpe_test_s', NRPEServerTest.simple_handler, 'TODO')
		self.reg.function('check_py_nrpe_test', NRPEServerTest.handler, 'TODO')

	def simple_handler(arguments):
		instance = NRPEServerTest.getInstance()
		return instance.simple_handler_wrapped(arguments)
	simple_handler = Callable(simple_handler)

	def handler(channel, request):
		instance = NRPEServerTest.getInstance()
		return instance.handler_wrapped(channel, request)
	handler = Callable(handler)
	
	def simple_handler_wrapped(self, arguments):
		log('Got simple message %s'%arguments)
		msg = self.get_response(arguments[0])
		msg.got_simple_response = True
		self.set_response(msg)
		rmsg = self.get_request(arguments[0])
		return (rmsg.status, rmsg.message, rmsg.perfdata)

	def handler_wrapped(self, channel, request):
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
		
	def submit_payload(self, alias, ssl, length, source, status, msg, perf, target):
		message = plugin_pb2.QueryRequestMessage()
		
		message.header.destination_id = target
		message.header.command = 'nrpe_forward'
		host = message.header.hosts.add()
		host.address = "127.0.0.1:15666"
		host.id = target
		if (target == 'valid'):
			pass
		else:
			enc = host.metadata.add()
			enc.key = "use ssl"
			enc.value = '%s'%ssl
			enc = host.metadata.add()
			enc.key = "payload length"
			enc.value = '%d'%length
		enc = host.metadata.add()
		enc.key = "timeout"
		enc.value = '5'

		uid = str(uuid.uuid4())
		payload = message.payload.add()
		payload.command = 'check_py_nrpe_test_s'
		payload.arguments.append(uid)
		rmsg = self.get_request(uid)
		rmsg.status = status
		rmsg.message = msg
		rmsg.perfdata = perf
		self.set_request(rmsg)
		(result_code, response) = self.core.query('ignored', message.SerializeToString())
		response_message = plugin_pb2.QueryResponseMessage()
		response_message.ParseFromString(response)
		result = TestResult('Testing NRPE: %s for %s'%(alias, target))
		
		found = False
		for i in range(0,10):
			if self.has_response(uid):
				rmsg = self.get_response(uid)
				#result.add_message(rmsg.got_response, 'Testing to recieve message using %s'%alias)
				result.add_message(rmsg.got_simple_response, 'Testing to recieve simple message using %s'%alias)
				result.add_message(len(response_message.payload) == 1, 'Verify that we only get one payload response for %s'%alias, '%s != 1'%len(response_message.payload))
				if len(response_message.payload) == 1 and len(response_message.payload[0].lines) == 1:
					result.assert_equals(response_message.payload[0].result, status, 'Verify that status is sent through %s'%alias)
					result.assert_equals(response_message.payload[0].lines[0].message, msg, 'Verify that message is sent through %s'%alias)
					#result.assert_equals(rmsg.perfdata, perf, 'Verify that performance data is sent through')
				self.del_response(uid)
				found = True
				break
			else:
				log('Waiting for %s (%s/%s)'%(uid,alias,target))
				sleep(500)
		if not found:
			result.add_message(False, 'Testing to recieve message using %s'%alias)
		return result

	def test_one(self, ssl=True, length=1024, state = status.UNKNOWN, tag = 'TODO'):
		result = TestResult('Testing NRPE: %s/%s/%s with various targets'%(ssl, length, tag))
		for t in ['valid', 'test_rp', 'invalid']:
			result.add(self.submit_payload('%s/%s/%s'%(ssl, length, tag), ssl, length, '%ssrc%s'%(tag, tag), state, '%smsg%s'%(tag, tag), '', t))
		return result

	def do_one_test(self, ssl=True, length=1024):
		conf = self.conf
		conf.set_int('/settings/NRPE/test_nrpe_server', 'payload length', length)
		conf.set_bool('/settings/NRPE/test_nrpe_server', 'use ssl', ssl)
		conf.set_bool('/settings/NRPE/test_nrpe_server', 'allow arguments', True)
		# TODO: conf.set_string('/settings/NRPE/test_nrpe_server', 'certificate', ssl)
		self.core.reload('test_nrpe_server')

		conf.set_string('/settings/NRPE/test_nrpe_client/targets/default', 'address', 'nrpe://127.0.0.1:35666')
		conf.set_bool('/settings/NRPE/test_nrpe_client/targets/default', 'use ssl', not ssl)
		conf.set_int('/settings/NRPE/test_nrpe_client/targets/default', 'payload length', length*3)

		conf.set_string('/settings/NRPE/test_nrpe_client/targets/invalid', 'address', 'nrpe://127.0.0.1:25666')
		conf.set_bool('/settings/NRPE/test_nrpe_client/targets/invalid', 'use ssl', not ssl)
		conf.set_int('/settings/NRPE/test_nrpe_client/targets/invalid', 'payload length', length*2)

		conf.set_string('/settings/NRPE/test_nrpe_client/targets/valid', 'address', 'nrpe://127.0.0.1:15666')
		conf.set_bool('/settings/NRPE/test_nrpe_client/targets/valid', 'use ssl', ssl)
		conf.set_int('/settings/NRPE/test_nrpe_client/targets/valid', 'payload length', length)
		self.core.reload('test_nrpe_client')
		
		result = TestResult('ssl=%s, length=%s'%(ssl, length))
		result.add_message(isOpen('127.0.0.1', 15666), 'Checking that port is open (server is up)')
		result.add(self.test_one(ssl, length, state = status.UNKNOWN, tag = 'unknown'))
		result.add(self.test_one(ssl, length, state = status.OK, tag = 'ok'))
		result.add(self.test_one(ssl, length, state = status.WARNING, tag = 'warn'))
		result.add(self.test_one(ssl, length, state = status.CRITICAL, tag = 'crit'))
		return result

	def run_test(self):
		result = TestResult()
		result.add(self.do_one_test(ssl=True))
		result.add(self.do_one_test(ssl=False))
		result.add(self.do_one_test(ssl=True, length=4096))
		result.add(self.do_one_test(ssl=True, length=65536))
		result.add(self.do_one_test(ssl=True, length=1048576))
		return result
		
	def install(self, arguments):
		conf = self.conf
		conf.set_string('/modules', 'test_nrpe_server', 'NRPEServer')
		conf.set_string('/modules', 'test_nrpe_client', 'NRPEClient')
		conf.set_string('/modules', 'pytest', 'PythonScript')

		conf.set_string('/settings/pytest/scripts', 'test_nrpe', 'test_nrpe.py')
		
		conf.set_string('/settings/NRPE/test_nrpe_server', 'port', '15666')
		conf.set_string('/settings/NRPE/test_nrpe_server', 'inbox', 'nrpe_test_inbox')
		conf.set_string('/settings/NRPE/test_nrpe_server', 'encryption', '1')

		conf.set_string('/settings/NRPE/test_nrpe_client/targets', 'nrpe_test_local', 'nrpe://127.0.0.1:15666')
		conf.set_string('/settings/NRPE/test_nrpe_client', 'channel', 'nrpe_test_outbox')
		
		conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def shutdown(self):
		None

	def require_boot(self):
		return True

setup_singleton(NRPEServerTest)

all_tests = [NRPEServerTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
