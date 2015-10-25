from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
import uuid
import os


def create_test_data(file):
	with open(file, "w") as f:
		f.write("1,A,Test 1\n")
		f.write("2,A,Test 2\n")
		f.write("3,B,Test 1\n")
		f.write("4,B,Test 1\n")
		f.write("5,C,Test 1\n")
		f.write("6,B,Test 2\n")

def delete_file(file):
	if os.path.exists(file):
		try:
			os.remove(file)
		except OSError, (errno, strerror):
			log('Failed to delete: %s'%file)

class LogFileTest(BasicTest):

	reg = None
	conf = None
	core = None

	def __init__(self):
		self.temp_path = None
		self.work_file = None
		None

	def desc(self):
		return 'Testcase for check_file module'

	def title(self):
		return 'Win32File tests'

	def setup(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)
		self.temp_path = self.core.expand_path('${temp}')
		log('Temp: %s'%self.temp_path)
		self.work_path = os.path.join(self.temp_path, '%s.txt'%uuid.uuid4())
		log('Work: %s'%self.work_path)
		create_test_data(self.work_path)

	def teardown(self):
		delete_file(self.work_path)

	def get_count(self,perf):
		if not perf:
			return -1
		(title, data) = perf.split('=')
		if not data:
			return -1
		(count, warn, crit) = data.split(';')
		return int(count)

	def check_files(self, filter, text, expected):
		alias = '%s: %s'%(text, filter)
		result = TestResult('Checking %s'%alias)
		args = ['file=%s'%self.work_path, 'column-split=,', 'filter=%s'%filter, 'warn=count gt %d'%expected, 'crit=count gt %d'%expected]
		#log("Command: %s"%args)
		(ret, msg, perf) = self.core.simple_query('check_logfile', args)
		log("%s : %s -- %s"%(filter, msg, perf))
		count = self.get_count(perf)
		result.add_message(count == expected, '%s - number of files'%filter, 'got %s expected %s'%(count, expected))
		result.add_message(ret == status.OK, '%s -- status', 'got %s expected OK'%ret)
		return result

	def check_bound(self, filter, warn, crit, expected):
		alias = '%s/%s/%s'%(filter, warn, crit)
		result = TestResult('Checking %s'%alias)
		args = ['file=%s'%self.work_path, 'column-split=,', 'filter=%s'%filter, 'warn=%s'%warn, 'crit=%s'%crit]
		#log("Command: %s"%args)
		(ret, msg, perf) = self.core.simple_query('check_logfile', args)
		log("%s : %s -- %s"%(filter, msg, perf))
		result.add_message(ret == expected, 'Check status', 'Invalid check status: %s'%ret)
		return result
		
	def run_filter_operator_test(self):
		result = TestResult('Filter tests')
		result.add(self.check_files('none', 'Count all lines', 7))
		result.add(self.check_files("column2 = 'A'", 'Count all A', 2))
		result.add(self.check_files("column2 = 'B'", 'Count all B', 3))
		result.add(self.check_files("column2 = 'C'", 'Count all C', 1))
		result.add(self.check_files("column3 = 'Test 1'", 'Count all T1', 4))
		result.add(self.check_files("column3 like 'Test'", 'Count all T', 6))
		result.add(self.check_files("column3 not like '1'", 'Count all T', 3))
		result.add(self.check_files("column1 > 1", 'Count all B', 5))
		result.add(self.check_files("column1 > 3", 'Count all B', 3))
		result.add(self.check_files("column1 > 5", 'Count all B', 1))
		result.add(self.check_files("column1 < 1", 'Count all B', 1))
		result.add(self.check_files("column1 < 3", 'Count all B', 3))
		result.add(self.check_files("column1 < 5", 'Count all B', 5))
		result.add(self.check_files("column1 = 1", 'Count all B', 1))
		result.add(self.check_files("column1 = 3", 'Count all B', 1))
		result.add(self.check_files("column1 = 5", 'Count all B', 1))
		result.add(self.check_files("column1 != 1", 'Count all B', 6))
		result.add(self.check_files("column1 != 3", 'Count all B', 6))
		result.add(self.check_files("column1 != 5", 'Count all B', 6))
		
		return result

	def run_boundry_test(self):
		result = TestResult('Boundry tests')
		result.add(self.check_bound('none', 'count > 1', 'none', status.WARNING))
		result.add(self.check_bound('none', 'none', 'count > 1', status.CRITICAL))
		result.add(self.check_bound('column1 > 5', 'count > 2', 'count > 5', status.OK))
		result.add(self.check_bound('column1 > 4', 'count > 2', 'count > 5', status.OK))
		result.add(self.check_bound('column1 > 3', 'count > 2', 'count > 5', status.WARNING))
		result.add(self.check_bound('column1 > 2', 'count > 2', 'count > 5', status.WARNING))
		result.add(self.check_bound('column1 > 1', 'count > 2', 'count > 5', status.WARNING))
		result.add(self.check_bound('column1 > 0', 'count > 2', 'count > 5', status.CRITICAL))

		result.add(self.check_bound('column1 > 5', 'column1 = 3', 'none', status.OK))
		result.add(self.check_bound('column1 > 0', 'column1 = 3', 'none', status.WARNING))
		
		return result

	def run_test(self):
		result = TestResult('Test')
		result.append(self.run_filter_operator_test())
		result.append(self.run_boundry_test())
		return result

	def install(self, arguments):
		conf = self.conf
		conf.set_string('/modules', 'test_disk', 'CheckLogFile')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/settings/pytest/scripts', 'test_logfile', 'test_log_file.py')
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

setup_singleton(LogFileTest)

all_tests = [LogFileTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
