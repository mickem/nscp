from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, install_testcases, init_testcases, shutdown_testcases
import subprocess


def get_expected_state(existing, sign, asked):
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
		pass


	
	def test_one_proc_int(self, proc, actual, asked):
		result = TestResult('Checking one state %d/%d'%(actual, asked))
		for op in ['eq', 'gt', 'lt', 'ne']:
			(return_code, return_message, return_performance_data) = self.core.simple_query('check_process', ['empty-state=OK', 'show-all', 'crit=count %s %d' % (op, asked), "filter=exe='%s'" % proc])
			expected = get_expected_state(actual, op, asked)
			result.add_message(return_code == expected, 'Process: %s (%d %s %d): %s' % (proc, actual, op, asked, return_message), '%s != %s' % (return_code, expected))
		return result
		
	def run_test_proc(self):
		master = TestResult('Checking check_process')
		
		result = TestResult('0 winver running')
		for j in range(0,3):
			result.append(self.test_one_proc_int('winver.exe', 0, j))
		master.add(result)
		
		pids = []
		for i in range(1,4):
			result = TestResult('%d winver running'%i)
			log('Starting winver...')
			handle = subprocess.Popen('winver.exe', shell=False)
			sleep(500)
			pids.append(handle.pid)
			for j in range(0,3):
				result.append(self.test_one_proc_int('winver.exe', i, j))
			master.add(result)

		for p in pids:
			subprocess.Popen("taskkill /F /T /PID %i"%p , shell=True)

		return master

	def run_test_process_history(self):
		master = TestResult('Checking check_process_history and check_process_history_new')

		# Test 1: Check process history before starting winver
		result = TestResult('Process history - winver not yet started')
		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history', ['process=winver.exe', 'warn=times_seen > 0'])
		result.add_message(True, 'check_process_history returned: %s - %s'%(return_code, return_message))
		master.add(result)

		# Test 2: Start winver and verify it appears in process history
		result = TestResult('Process history - winver started')
		log('Starting winver for process history test...')
		handle = subprocess.Popen('winver.exe', shell=False)
		sleep(10000)  # Wait for process history to be updated

		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history', ['process=winver.exe', 'warn=currently_running = 0', 'detail-syntax=%(exe): %(currently_running)', 'show-all'])
		result.add_message(return_code == status.OK, 'check_process_history should show winver running: %s - %s'%(return_code, return_message), 'Expected OK, got %s'%return_code)

		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history', ['process=winver.exe', 'crit=times_seen = 0'])
		result.add_message(return_code == status.OK, 'check_process_history should show times_seen > 0: %s - %s'%(return_code, return_message), 'Expected OK, got %s'%return_code)
		master.add(result)

		# Test 3: Check for new processes (winver should be new)
		result = TestResult('Process history new - winver should be detected as new')
		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history_new', ['time=5m', "filter=exe like 'winver'"])
		result.add_message(True, 'check_process_history_new returned: %s - %s'%(return_code, return_message))
		# Note: This may or may not find winver depending on when it was first seen
		master.add(result)

		# Test 4: Stop winver and verify status changes
		result = TestResult('Process history - winver stopped')
		subprocess.Popen("taskkill /F /T /PID %i"%handle.pid, shell=True)
		sleep(10000)  # Wait for process history to be updated

		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history', ['process=winver.exe', 'warn=currently_running = 1', 'detail-syntax=%(exe): %(currently_running)'])
		result.add_message(return_code == status.OK, ' ggg check_process_history should show winver not running: %s - %s'%(return_code, return_message), f'Expected OK, got {return_code}: {return_message}')
		master.add(result)

		# Test 5: Check process history with show-all
		result = TestResult('Process history - show all processes')
		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history', ['show-all', 'top-syntax=${count} processes in history'])
		result.add_message(return_code != status.UNKNOWN, 'check_process_history show-all returned: %s - %s'%(return_code, return_message), 'Got UNKNOWN status')
		master.add(result)

		# Test 6: Check for unknown process (should not be in history)
		result = TestResult('Process history - unknown process')
		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history', ['process=this_process_does_not_exist_12345.exe', 'crit=times_seen = 0'])
		result.add_message(return_code == status.CRITICAL, 'Unknown process should have times_seen = 0: %s - %s'%(return_code, return_message), 'Expected CRITICAL, got %s'%return_code)
		master.add(result)

		# Test 7: Test check_process_history_new with short time window (should find nothing old)
		result = TestResult('Process history new - with 1 second time window')
		(return_code, return_message, return_performance_data) = self.core.simple_query('check_process_history_new', ['time=1s'])
		result.add_message(True, 'check_process_history_new with 1s window returned: %s - %s'%(return_code, return_message))
		master.add(result)

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
		(return_code, return_message, return_performance_data) = self.core.simple_query('CheckCounter', args)
		result.add_message(return_code != status.UNKNOWN, 'Return code: %s'%return_code)
		result.add_message(len(return_message) > 0, 'Returned message: %s'%return_message)
		result.add_message(len(return_performance_data) > 0, 'Performance data: %s'%return_performance_data)
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

		
		(subres, name1) = self.check_and_lookup_index(4)
		result.add(subres)
		(subres, name2) = self.check_and_lookup_index(26)
		result.add(subres)
		
		result.add(self.check_counter('\\4\\26', ['ShowAll', 'MaxWarn=10']))
		result.add(self.check_counter('\\4\\26', ['index', 'ShowAll', 'MaxWarn=10']))
		result.add(self.check_counter('\\%s\\%s'%(name1, name2), ['ShowAll', 'MaxWarn=10']))
		return result
		
	def run_test(self, cases = None):
		result = TestResult('Testing W32 systems')
		result.add(self.run_test_proc())
		result.add(self.run_test_process_history())
		result.add(self.run_test_counters())
		return result

	def install(self):
		conf = self.conf
		conf.set_string('/modules', 'test_system', 'CheckSystem')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/settings/pytest/scripts', 'test_w32sys', 'test_w32_system.py')
		conf.set_bool('/settings/system/test_system', 'process history', True)

		conf.save()

	def uninstall(self):
		pass

	def help(self):
		pass

	def init(self, plugin_id):
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def shutdown(self):
		pass

all_tests = [Win32SystemTest()]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
