from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
import random
import subprocess

class Win32SystemTest(BasicTest):

	reg = None
	conf = None
	core = None

	def desc(self):
		return 'Testcase for w32 check_system module'

	def title(self):
		return 'Win32System tests'

	def setup(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)

	def teardown(self):
		None

	def get_expected_state(self, existing, sign, asked):
		if existing == 0:
			return status.OK
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
			(retcode, retmessage, retperf) = self.core.simple_query('check_process', ['empty-state=OK', 'show-all', 'crit=count %s %d'%(s, asked), "filter=exe='%s'"%proc])
			expected = self.get_expected_state(actual, s, asked)
			result.add_message(retcode == expected, 'Process: %s (%d %s %d): %s'%(proc, actual, s, asked, retmessage), '%s != %s'%(retcode, expected))
		return result
		
	def run_test_proc(self):
		master = TestResult('Checking check_process')
		
		result = TestResult('0 notepads running')
		for j in range(0,3):
			result.append(self.test_one_proc_int('notepad.exe', 0, j))
		master.add(result)
		
		pids = []
		for i in range(1,4):
			result = TestResult('%d notepads running'%i)
			log('Starting notepad...')
			handle = subprocess.Popen('notepad.exe', shell=False)
			sleep(500)
			pids.append(handle.pid)
			for j in range(0,3):
				result.append(self.test_one_proc_int('notepad.exe', i, j))
			master.add(result)

		for p in pids:
			subprocess.Popen("taskkill /F /T /PID %i"%p , shell=True)

		return master
		
	def check_and_lookup_index(self, index):
		result = TestResult('Validating index: %s'%index)
		(result_code, result_message) = self.core.simple_exec('any', 'pdh', ['--lookup-name', '%s'%index, '--porcelain'])
		result.assert_equals(result_code, 0, 'Result code')
		result.assert_equals(len(result_message), 1, 'result length')
		result.add_message(len(result_message[0])>0, 'result length')
		name = result_message[0]
		
		(result_code, result_message) = self.core.simple_exec('any', 'pdh', ['--lookup-index', name, '--porcelain'])
		result.assert_equals(result_code, 0, 'Result code')
		result.assert_equals(len(result_message), 1, 'result length')
		result.add_message(len(result_message[0])>0, 'result length')
		result.assert_equals(result_message[0], '%s'%index, 'result length')
	
		return result, name
		
	def check_counter(self, counter, args):
		result = TestResult('Checking counter: %s'%counter)
		args.append('Counter=%s'%counter)
		(retcode, retmessage, retperf) = self.core.simple_query('CheckCounter', args)
		result.add_message(retcode != status.UNKNOWN, 'Return code: %s'%retcode)
		result.add_message(len(retmessage) > 0, 'Returned message: %s'%retmessage)
		result.add_message(len(retperf) > 0, 'Performance data: %s'%retperf)
		return result
	
	def run_test_counters(self):
		result = TestResult('Checking CheckCounter')
		(result_code, result_message) = self.core.simple_exec('any', 'pdh', ['--list', '--all'])
		count = 0
		data = []
		for m in result_message:
			data = m.splitlines()
			count = len(data)
		result.add_message(count > 0, 'Managed to retrieve counters: %d'%count)
		if len(data) == 0:
			result.add_message(False, 'Failed to find counters: %s'%result_message)
		counters = []
		
		
		(subres, name1) = self.check_and_lookup_index(4)
		result.add(subres)
		(subres, name2) = self.check_and_lookup_index(26)
		result.add(subres)
		
		result.add(self.check_counter('\\4\\26', ['ShowAll', 'MaxWarn=10']))
		result.add(self.check_counter('\\4\\26', ['index', 'ShowAll', 'MaxWarn=10']))
		result.add(self.check_counter('\\%s\\%s'%(name1, name2), ['ShowAll', 'MaxWarn=10']))
		return result
		
	def run_test(self):
		result = TestResult('Testing W32 systems')
		result.add(self.run_test_proc())
		result.add(self.run_test_counters())
		return result

	def install(self, arguments):
		conf = self.conf
		conf.set_string('/modules', 'test_system', 'CheckSystem')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/settings/pytest/scripts', 'test_w32sys', 'test_w32_system.py')
		conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def shutdown(self):
		None

setup_singleton(Win32SystemTest)

all_tests = [Win32SystemTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
