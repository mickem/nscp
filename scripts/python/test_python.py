from NSCP import Settings, Registry, Core, log, status, log_error, log_debug, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
from time import time

install_checks = 100
time_to_run = 30

class PythonTest(BasicTest):

	noop_count = 0
	stress_count = 0
	
	key = ''
	reg = None
	conf = None
	core = None

	def noop_handler(arguments):
		instance = PythonTest.getInstance()
		instance.noop_count = instance.noop_count + 1
		return (status.OK, 'Got call %d'%instance.noop_count, '')
	noop_handler = Callable(noop_handler)

	
	def stress_handler(channel, source, command, code, message, perf):
		instance = PythonTest.getInstance()
		instance.stress_count = instance.stress_count + 1
		log_debug('Got message %d/%d on %s'%(instance.stress_count, instance.noop_count, channel))
	stress_handler = Callable(stress_handler)
	
	def desc(self):
		return 'Testcase for python script module'

	def title(self):
		return 'PythonScript tests'

	def setup(self, plugin_id, prefix):
		log('Loading Python unit tests')
		self.key = '_%stest_command'%prefix
		self.reg.simple_function('py_stress_noop', PythonTest.noop_handler, 'This is a simple noop command')
		self.reg.simple_subscription('py_stress_test', PythonTest.stress_handler)
		self.conf.set_string('/settings/test_scheduler', 'threads', '50')
		self.core.reload('test_scheduler')
		

	def teardown(self):
		self.conf.set_string('/settings/test_scheduler', 'threads', '0')
		self.core.reload('test_scheduler')
		None

	def run_test(self):
		result = TestResult()
		start = time()
		total_count = install_checks*time_to_run/5
		while self.stress_count < total_count:
			log('Waiting for %d: %d/%d'%(total_count, self.stress_count, self.noop_count))
			old_stress_count = self.stress_count
			old_noop_count = self.noop_count
			sleep(5000)
			result.add_message(True, 'Commands/second: %d/%d'%( (self.stress_count-old_stress_count)/5, (self.noop_count-old_noop_count)/5 ) )
		elapsed = (time() - start)
		if elapsed == 0:
			elapsed = 1
		result.add_message(True, 'Summary Collected %d instance in %d seconds: %d/s'%(self.stress_count, elapsed, self.stress_count/elapsed))
		return result

	def install(self, arguments):
		self.conf.set_string('/modules', 'test_scheduler', 'Scheduler')
		self.conf.set_string('/modules', 'pytest', 'PythonScript')

		self.conf.set_string('/settings/pytest/scripts', 'test_python', 'test_python.py')
		
		base_path = '/settings/test_scheduler'
		self.conf.set_string(base_path, 'threads', '0')

		default_path = '%s/schedules/default'%base_path
		self.conf.set_string(default_path, 'channel', 'py_stress_test')
		#self.conf.set_string(default_path, 'alias', 'stress')
		self.conf.set_string(default_path, 'command', 'py_stress_noop')
		self.conf.set_string(default_path, 'interval', '5s')
		for i in range(1, install_checks):
			alias = 'stress_python_%i'%i
			self.conf.set_string('%s/schedules'%(base_path), alias, 'py_stress_noop')

		self.conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

		None

	def shutdown(self):
		None

setup_singleton(PythonTest)

all_tests = [PythonTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()

