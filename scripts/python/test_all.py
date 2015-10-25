from NSCP import Settings, Registry, Core, log, status, log_error
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases

from sys import path
import os
path.append(os.getcwd() + '/scripts/python')

from test_nsca import NSCAServerTest
from test_nrpe import NRPEServerTest
from test_python import PythonTest

# 
all_tests = [NSCAServerTest, PythonTest, NRPEServerTest]
if os.name == 'nt':
	from test_eventlog import EventLogTest
	from test_w32_system import Win32SystemTest
	from test_w32_file import Win32FileTest
	from test_w32_wmi import Win32WMITest
	from test_external_script import ExternalScriptTest
	#all_tests.extend([EventLogTest, Win32SystemTest, Win32FileTest, Win32WMITest])
	all_tests.extend([Win32SystemTest, Win32FileTest, Win32WMITest, ExternalScriptTest])

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
