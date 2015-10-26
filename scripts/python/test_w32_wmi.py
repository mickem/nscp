from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
import random
import subprocess
import uuid
import os
import sys, stat, datetime, time

class Win32WMITest(BasicTest):

	reg = None
	conf = None
	core = None

	def desc(self):
		return 'Testcase for w32 check_wmi module'

	def title(self):
		return 'Win32File tests'

	def setup(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)

	def teardown(self):
		None
	
	def get_count(self,perf):
		if not perf:
			return -1
		(title, data) = perf.split('=')
		if not data:
			return -1
		(count, warn, crit) = data.split(';')
		return int(count)

		
	def check_cli_ns(self):
		result = TestResult('Checking CLI list-ns')
		(ret, ns_msgs) = self.core.simple_exec('any', 'wmi', ['--list-all-ns', '--namespace', 'root'])
		result.assert_equals(ret, 0, 'Check that --list-all-ns returns ok')
		result.assert_equals(len(ns_msgs), 1, 'Check that --list-all-ns returns one entry')
		if len(ns_msgs) > 0:
			result.assert_contains(ns_msgs[0], 'CIMV2', 'Check that --list-all-ns contains cimv2')
		return result

	def check_cli_ls(self, ns, expected, missing):
		result = TestResult('Checking CLI list-classes %s'%ns)
		args = ['--list-classes', '--simple']
		if ns != None:
			args.extend(['--namespace', ns])
		(ret, ns_msgs) = self.core.simple_exec('any', 'wmi', args)
		result.assert_equals(ret, 0, 'Check that --list-classes returns ok')
		result.assert_equals(len(ns_msgs), 1, 'Check that --list-classes returns one entry')
		if len(ns_msgs) > 0:
			result.assert_contains(ns_msgs[0], expected, 'Check that --list-classes contains %s'%expected)
			result.assert_not_contains(ns_msgs[0], missing, 'Check that --list-classes does not contains %s'%missing)
		return result

	def check_cli_query(self, query, count, check, ns = None):
		result = TestResult('Checking CLI query %s'%query)
		args = ['--select', query, '--simple']
		if ns != None:
			args.extend(['--namespace', ns])
		(ret, ns_msgs) = self.core.simple_exec('any', 'wmi', args)
		result.assert_equals(ret, 0, 'Check that --select returns ok')
		result.assert_equals(len(ns_msgs), 1, 'Check that --select returns one entry')
		if len(ns_msgs) > 0:
			result.add_message(count(ns_msgs[0].splitlines()), 'Check that it contains the right number of rows')
			result.add_message(check(ns_msgs[0]), 'Check that it contains the right data')
		return result
		
	def run_test(self):
		result = TestResult('Testing W32 file systems')
		result.add(self.check_cli_ns())
		result.add(self.check_cli_ls(None, 'Win32_Processor', 'LogFileEventConsumer'))
		result.add(self.check_cli_ls('root\subscription', 'LogFileEventConsumer', 'Win32_Processor'))
		result.add(self.check_cli_query('SELECT DeviceId, AddressWidth, Caption, Name FROM Win32_Processor', lambda x:x>1, lambda x:'CPU0' in x))
		return result

	def install(self, arguments):
		conf = self.conf
		conf.set_string('/modules', 'test_wmi', 'CheckWMI')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/settings/pytest/scripts', 'test_w32wmi', 'test_w32_wmi.py')
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

setup_singleton(Win32WMITest)

all_tests = [Win32WMITest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
