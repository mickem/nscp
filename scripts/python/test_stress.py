from NSCP import Settings, Registry, Core, log, status, log_debug, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
from time import time
import random
import os

is_windows = False
if os.name == 'nt':
	is_windows = True

check_per_second = 10
#time_to_run = 'infinate'
time_to_run = 60*10
use_threads = 10

route_via_nsca = True
route_via_python = False

prefix = 'stress'

core = Core.get()

class StressTest(BasicTest):

	check_count = 0
	results_count = 0
	
	sched_alias = 'test_sched_%s'%prefix
	nsca_server_alias = 'test_nsca_s_%s'%prefix
	nsca_client_alias = 'test_nsca_c_%s'%prefix
	python_channel = 'test_stress_%s_py'%prefix
	nsca_channel = 'test_stress_%s_nsca'%prefix
	command = 'test_stress_%s'%prefix
	port = 15568
	sched_base_path = '/settings/%s'%sched_alias
	
	background = False


	checks = [
			['CheckCPU', ['MaxWarn=20', 'MaxCrit=20']],
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
			check = self.get_random_check()
			self.check_count = self.check_count + 1
			return core.simple_query(check[0], check[1])
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
		self.reg = Registry.get(plugin_id)
		self.reg.simple_function(self.command, StressTest.random_check_handler, 'This is a simple noop command')
		self.reg.simple_subscription(self.python_channel, StressTest.on_stress_handler)
		conf = Settings.get()
		conf.set_string(self.sched_base_path, 'threads', '%d'%use_threads)
		core.reload(self.sched_alias)
		

	def teardown(self):
		if not self.background:
			conf = Settings.get()
			conf.set_string(self.sched_base_path, 'threads', '0')
			core.reload(self.sched_alias)

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
		conf = Settings.get()

		# Configure required modules
		if route_via_python:
			conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/modules', self.sched_alias, 'Scheduler')
		if is_windows:
			conf.set_string('/modules', 'CheckSystem', 'enabled')
		conf.set_string('/modules', 'CheckHelpers', 'enabled')

		if route_via_nsca:
			conf.set_string('/modules', self.nsca_server_alias, 'NSCAServer')
			conf.set_string('/modules', self.nsca_client_alias, 'NSCAClient')
			
			# Configure NSCA Server
			conf.set_string('/settings/NSCA/%s'%self.nsca_server_alias, 'port', '%d'%self.port)
			conf.set_string('/settings/NSCA/%s'%self.nsca_server_alias, 'inbox', self.python_channel)
			conf.set_string('/settings/NSCA/%s'%self.nsca_server_alias, 'encryption', 'aes')

			# Configure NSCA Client
			conf.set_string('/settings/NSCA/%s/targets/default'%self.nsca_client_alias, 'address', 'nsca://127.0.0.1:%d'%self.port)
			conf.set_string('/settings/NSCA/%s/targets/default'%self.nsca_client_alias, 'encryption', 'aes')
			conf.set_string('/settings/NSCA/%s'%self.nsca_client_alias, 'channel', self.nsca_channel)
		
		# Configure python
		if route_via_python:
			conf.set_string('/settings/pytest/scripts', 'test_stress', 'test_stress.py')
		
		# Configure Scheduler
		if route_via_python:
			conf.set_string(self.sched_base_path, 'threads', '0')
		else:
			conf.set_string(self.sched_base_path, 'threads', '50')

		default_path = '%s/default'%self.sched_base_path
		if route_via_nsca:
			conf.set_string(default_path, 'channel', self.nsca_channel)
		else:
			conf.set_string(default_path, 'channel', self.python_channel)

		conf.set_string(default_path, 'alias', 'stress')
		#conf.set_string(default_path, 'target', 'stress_001')
		
		use_command = self.command
		if not route_via_python:
			use_command = 'CheckOK'
		
		conf.set_string(default_path, 'command', use_command)
		conf.set_string(default_path, 'interval', '5s')
		for i in range(1, (check_per_second*5)+1):
			alias = 'stress_python_%i'%i
			conf.set_string('%s/schedules'%(self.sched_base_path), alias, use_command)

		conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		None

	def shutdown(self):
		None

setup_singleton(StressTest)

all_tests = [StressTest]

def __main__():
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
