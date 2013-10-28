from NSCP import Settings, Registry, Core, log, status, log_debug, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
from time import time
import random
import os

is_windows = False
if os.name == 'nt':
	is_windows = True

check_per_second = 1000
#time_to_run = 'infinate'
time_to_run = 60		# in seconds
use_threads = 100

route_via_nsca = True
route_via_nrpe = True
route_via_python = False

prefix = 'stress'


class StressTest(BasicTest):

	check_count = 0
	results_count = 0
	
	sched_alias = 'test_sched_%s'%prefix
	nsca_server_alias = 'test_nsca_s_%s'%prefix
	nsca_client_alias = 'test_nsca_c_%s'%prefix
	nrpe_server_alias = 'test_nrpe_s_%s'%prefix
	nrpe_client_alias = 'test_nrpe_c_%s'%prefix
	python_channel = 'test_stress_%s_py'%prefix
	nsca_channel = 'test_stress_%s_nsca'%prefix
	command = 'test_stress_%s'%prefix
	nsca_port = 15568
	nrpe_port = 15566
	sched_base_path = '/settings/%s'%sched_alias
	
	background = False


	checks = [
			['CheckCPU', ['MaxWarn=20', 'MaxCrit=20', '10s']],
			['CheckMEM', ['MaxWarn=20', 'MaxCrit=20']]
		]

	def get_random_check(self):
		return random.choice(self.checks)

	def random_check_handler(arguments):
		instance = StressTest.getInstance()
		return instance.wrapped_random_check_handler(arguments)
	random_check_handler = Callable(random_check_handler)

	def wrapped_random_check_handler(self, arguments):
		global is_windows
		if is_windows:
			check = []
			if route_via_nrpe:
				#  host=127.0.0.1 port=15566 command=CheckCPU arguments=MaxWarn=20 arguments=8s
				check_data = self.get_random_check()
				check[0] = 'nrpe_query'
				check[1] = ['host=127.0.0.1', 'port=%d'%self.nrpe_port, 'command=%s'%check_data[0]]
				for arg in check_data[1]:
					check[1].append('arguments=%s'%arg)
			else:
				check = self.get_random_check()
			self.check_count = self.check_count + 1
			return self.core.simple_query(check[0], check[1])
		else:
			return (status.OK, 'Got call %d'%(self.check_count), '')

	def on_stress_handler(channel, source, command, code, message, perf):
		instance = StressTest.getInstance()
		instance.wrapped_on_stress_handler(channel, source, command, code, message, perf)
	on_stress_handler = Callable(on_stress_handler)

	def wrapped_on_stress_handler(self, channel, source, command, code, message, perf):
		check = self.get_random_check()
		self.results_count = self.results_count + 1
		log_debug('Got result %s <%d/%d> on %s'%(message, self.results_count, self.check_count, channel))
		return None

	def desc(self):
		return 'Testcase for stresstest script module'

	def title(self):
		return 'StressTest tests'

	def setup(self, plugin_id, prefix):
		self.reg.simple_function(self.command, StressTest.random_check_handler, 'This is a simple noop command')
		self.reg.simple_subscription(self.python_channel, StressTest.on_stress_handler)
		self.conf.set_string(self.sched_base_path, 'threads', '%d'%use_threads)
		self.core.reload('%s,delayed'%self.sched_alias)
		

	def teardown(self):
		if not self.background:
			self.conf.set_string(self.sched_base_path, 'threads', '0')
			self.core.reload(self.sched_alias)

	def run_test(self):
		global time_to_run, check_per_second
		result = TestResult()
		start = time()
		if isinstance(time_to_run, str) and time_to_run == 'infinate':
			time_to_run = -1
		elif isinstance(time_to_run, str):
			time_to_run = 5

		if time_to_run == -1:
			total_count = -1
		else:
			total_count = check_per_second*time_to_run

		if time_to_run != -1:
			self.background = False
			last_major = 0
			while self.results_count < total_count:
				old_stress_count = self.results_count
				old_noop_count = self.check_count
				sleep(5000)
				result.add_message(True, 'Commands/second: %d/%d'%( (self.results_count-old_stress_count)/5, (self.check_count-old_noop_count)/5 ) )
				if (self.results_count*100/total_count) > last_major + 10:
					last_major = last_major + 10
					log('%d%% Complete: %d checks per second <%d/%d>'%(self.results_count*100/total_count, (self.results_count-old_stress_count)/5, self.results_count, total_count))
			elapsed = (time() - start)
			if elapsed == 0:
				elapsed = 1
			result.add_message(True, 'Summary Collected %d instance in %d seconds: %d/s'%(self.results_count, elapsed, self.results_count/elapsed))
		else:
			self.background = True
			result.add_message(True, 'Test running in background, run py_unittest_collect to collect results at any time.')
		return result

	def install(self, arguments):
		global is_windows, route_via_python, route_via_nsca, use_threads

		# Configure required modules
		self.conf.set_string('/modules', 'pytest', 'PythonScript')
		self.conf.set_string('/modules', self.sched_alias, 'Scheduler')
		if is_windows:
			self.conf.set_string('/modules', 'CheckSystem', 'enabled')
		self.conf.set_string('/modules', 'CheckHelpers', 'enabled')

		if route_via_nsca:
			self.conf.set_string('/modules', self.nsca_server_alias, 'NSCAServer')
			self.conf.set_string('/modules', self.nsca_client_alias, 'NSCAClient')
			
			# Configure NSCA Server
			self.conf.set_string('/settings/NSCA/%s'%self.nsca_server_alias, 'port', '%d'%self.nsca_port)
			self.conf.set_string('/settings/NSCA/%s'%self.nsca_server_alias, 'inbox', self.python_channel)
			self.conf.set_string('/settings/NSCA/%s'%self.nsca_server_alias, 'encryption', 'xor')
			self.conf.set_string('/settings/NSCA/%s'%self.nsca_server_alias, 'password', 'HelloWorld')

			# Configure NSCA Client
			self.conf.set_string('/settings/NSCA/%s/targets/default'%self.nsca_client_alias, 'address', 'nsca://127.0.0.1:%d'%self.nsca_port)
			self.conf.set_string('/settings/NSCA/%s/targets/default'%self.nsca_client_alias, 'encryption', 'xor')
			self.conf.set_string('/settings/NSCA/%s/targets/default'%self.nsca_client_alias, 'password', 'HelloWorld')
			self.conf.set_string('/settings/NSCA/%s'%self.nsca_client_alias, 'channel', self.nsca_channel)

		if route_via_nrpe:
			self.conf.set_string('/modules', self.nrpe_server_alias, 'NRPEServer')
			self.conf.set_string('/modules', self.nrpe_client_alias, 'NRPEClient')
			
			# Configure NRPE Server
			self.conf.set_string('/settings/NRPE/%s'%self.nrpe_server_alias, 'port', '%d'%self.nrpe_port)
			self.conf.set_string('/settings/NRPE/%s'%self.nrpe_server_alias, 'allow arguments', 'true')

			# Configure NRPE Client
			self.conf.set_string('/settings/NRPE/%s/targets/default'%self.nsca_client_alias, 'address', 'nrpe://127.0.0.1:%d'%self.nrpe_port)
			
		# Configure python
		self.conf.set_string('/settings/pytest/scripts', 'test_stress', 'test_stress.py')
		
		# Configure Scheduler
		if route_via_python:
			self.conf.set_string(self.sched_base_path, 'threads', '0')
		else:
			self.conf.set_string(self.sched_base_path, 'threads', '50')

		default_path = '%s/schedules/default'%self.sched_base_path
		if route_via_nsca:
			self.conf.set_string(default_path, 'channel', self.nsca_channel)
		else:
			self.conf.set_string(default_path, 'channel', self.python_channel)

		self.conf.set_string(default_path, 'alias', 'stress')
		#self.conf.set_string(default_path, 'target', 'stress_001')
		
		use_command = self.command
		if not route_via_python:
			if route_via_nrpe:
				use_command = 'nrpe_query host=127.0.0.1 port=%d command=CheckOK'%self.nrpe_port
			else:
				use_command = 'CheckOK'
		
		self.conf.set_string(default_path, 'command', use_command)
		self.conf.set_string(default_path, 'interval', '5s')
		log_debug('Adding %d checks'%int(check_per_second*5))
		for i in range(1, int(check_per_second*5)+1):
			
			alias = 'stress_python_%i'%i
			self.conf.set_string('%s/schedules'%(self.sched_base_path), alias, use_command)

		self.conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)
		self.conf = Settings.get(plugin_id)
		self.core = Core.get(plugin_id)
		None

	def shutdown(self):
		None

	def require_boot(self):
		return True

setup_singleton(StressTest)

all_tests = [StressTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
