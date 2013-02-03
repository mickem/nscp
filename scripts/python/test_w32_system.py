from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
import random
import subprocess

core = Core.get()

class Win32SystemTest(BasicTest):

	def desc(self):
		return 'Testcase for w32 check_system module'

	def title(self):
		return 'Win32System tests'

	def setup(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)

	def teardown(self):
		None

	def get_expected_state(self, existing, sign, asked):
		if sign == 'eq':
			if existing == asked:
				return status.CRITICAL
			return status.OK
		if sign == 'gt':
			if existing > asked:
				return status.CRITICAL
			return status.OK
		if sign == 'lt':
			if existing < asked:
				return status.CRITICAL
			return status.OK
		if sign == 'ne':
			if existing != asked:
				return status.CRITICAL
			return status.OK
		return status.UNKNOWN
	
	def test_one_proc_int(self, proc, actual, asked):
		result = TestResult('Checking one state %d/%d'%(actual, asked))
		for s in ['eq', 'gt', 'lt', 'ne']:
			(retcode, retmessage, retperf) = core.simple_query('CheckProcState', ['ShowAll', 'critCount=%s:%d'%(s, asked), '%s=started'%proc])
			expected = self.get_expected_state(actual, s, asked)
			result.add_message(retcode == expected, 'Process: %s (%d %s %d): %s'%(proc, actual, s, asked, retmessage), 'Expected %s'%(expected))
		return result
		
	def run_test_proc(self):
		result = TestResult('Checking CheckProcState')
		
		for j in range(0,3):
			result.append(self.test_one_proc_int('notepad.exe', 0, j))
		
		pids = []
		for i in range(1,4):
			log('Starting notepad...')
			handle = subprocess.Popen('notepad.exe', shell=False)
			sleep(500)
			pids.append(handle.pid)
			for j in range(0,3):
				result.append(self.test_one_proc_int('notepad.exe', i, j))

		for p in pids:
			subprocess.Popen("taskkill /F /T /PID %i"%p , shell=True)

		return result
		
	def run_test_counters(self):
		result = TestResult('Checking CheckCounter')
		(result_code, result_message) = core.simple_exec('any', 'pdh', ['--list', '--porcelain'])
		count = 0
		data = []
		for m in result_message:
			data = m.splitlines()
			count = len(data)
		result.add_message(count > 0, 'Managed to retrieve counters: %d'%count)
		if len(data) == 0:
			result.add_message(False, 'Failed to find counters: %s'%result_message)
		counters = []
		for x in range(1,10):
			try:
				str = random.choice(data)
				(alias, counter, message) = str.split(',', 2)
				(retcode, retmessage, retperf) = core.simple_query('CheckCounter', ['index', 'ShowAll', 'MaxWarn=10', 'Counter:001=%s'%counter])
				result.add_message(retcode != status.UNKNOWN, 'Queried normal: %s'%counter)
				result.add_message(len(retmessage) > 0, 'Queried normal (got message): %s'%retmessage)
				result.add_message(len(retperf) > 0, 'Queried normal (got perf): %s'%retperf)
				if retcode != status.UNKNOWN:
					counters.append('Counter:%d=%s'%(x, counter))
			except Exception as e:
				result.add_message(False, 'Invalid counter found "%s": %s'%(str, e))

		args = ['index', 'ShowAll', 'MaxWarn=10']
		args.extend(counters)
		(retcode, retmessage, retperf) = core.simple_query('CheckCounter', args)
		result.add_message(retcode != status.UNKNOWN, 'Queried normal list of %d counters'%len(counters))
		result.add_message(len(retmessage) > 0, 'Queried normal (got message): %s'%retmessage)
		result.add_message(len(retperf) > 0, 'Queried normal (got perf): %s'%retperf)
		result.add_message(len(counters) == len(retperf.split(' ')), 'Got all responses: %d'%len(counters))
		return result
		
	def run_test(self):
		result = TestResult('Testing W32 systems')
		#result.add(self.run_test_proc())
		result.add(self.run_test_counters())
		return result

	def install(self, arguments):
		conf = Settings.get()
		conf.set_string('/modules', 'test_system', 'CheckSystem')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/settings/pytest/scripts', 'test_w32sys', 'test_w32_system.py')
		conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		None

	def shutdown(self):
		None

setup_singleton(Win32SystemTest)

all_tests = [Win32SystemTest]

def __main__():
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()

