from NSCP import Settings, Registry, Core, log, status, log_error, sleep
import sys, difflib

from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
import plugin_pb2
from types import *
import socket
import uuid
import unicodedata

import threading
sync = threading.RLock()

core = Core.get()


LONG_OUTPUT = """Test arguments are: (LONG "$ARG2$" "$ARG3$")\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"""

class ExternalScriptTest(BasicTest):
	instance = None
	key = ''
	reg = None
	_responses = {}
	_requests = {}
	
	def setup(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)

	def teardown(self):
		None
		
	def do_one_test(self, script, expected = status.OK, message = "Foo Bar", args=[]):
		result = TestResult('%s (%s)'%(script, args))
		(ret, msg, perf) = core.simple_query(script, args)
		result.assert_equals(ret, expected, 'Validate return code for %s'%script)
		result.assert_equals(msg, message, 'Validate return message for %s'%script)
		if msg != message:
			diff = difflib.ndiff(msg.splitlines(1), message.splitlines(1))
			print ''.join(diff),
		return result

	def run_test(self):
		ret = TestResult('External scripts test suite')
		result = TestResult('Arguments NOT allowed')
		result.add(self.do_one_test('tes_script_ok', message='OK: Everything is going to be fine'))
		result.add(self.do_one_test('tes_script_bat', message='Test arguments are: (  )'))
		
		result.add(self.do_one_test('tes_sa_bat', status.OK, 'Test arguments are: ("ARG1" "ARG 2" "A R G 3")'))
		
		result.add(self.do_one_test('tes_script_bat', status.UNKNOWN, 'Arguments not allowed', ['NOT ALLOWED']))
		
		ret.add(result)
		conf = Settings.get()
		conf.set_string('/settings/test_external_scripts', 'allow arguments', 'true')
		core.reload('test_external_scripts')

		result = TestResult('Arguments allowed')
		result.add(self.do_one_test('tes_sca_bat', status.OK, 'Test arguments are: (OK "$ARG2$" "$ARG3$")', ['OK']))
		result.add(self.do_one_test('tes_sca_bat', status.WARNING, 'Test arguments are: (WARN "$ARG2$" "$ARG3$")', ['WARN']))
		result.add(self.do_one_test('tes_sca_bat', status.CRITICAL, 'Test arguments are: (CRIT "$ARG2$" "$ARG3$")', ['CRIT']))
		result.add(self.do_one_test('tes_sca_bat', status.UNKNOWN, 'Test arguments are: (UNKNOWN "$ARG2$" "$ARG3$")', ['UNKNOWN']))

		result.add(self.do_one_test('tes_sca_bat', status.OK, 'Test arguments are: (OK "String with space" "A long long option with many spaces")', ['OK', 'String with space', 'A long long option with many spaces']))
		result.add(self.do_one_test('tes_sca_bat', status.OK, LONG_OUTPUT, ['LONG']))
		
		result.add(self.do_one_test('tes_sca_bat', status.UNKNOWN, 'Request contained illegal characters set /settings/external scripts/allow nasty characters=true!', ['OK', '$$$ \\ \\', '$$$ \\ \\']))

		ret.add(result)
		conf = Settings.get()
		conf.set_string('/settings/test_external_scripts', 'allow nasty characters', 'true')
		core.reload('test_external_scripts')

		result = TestResult('Nasty Arguments allowed')
		result.add(self.do_one_test('tes_sca_bat', status.OK, 'Test arguments are: (OK "$$$ \\ \\" "$$$ \\ \\")', ['OK', '$$$ \\ \\', '$$$ \\ \\']))
		
		ret.add(result)
		return ret
		
	def install(self, arguments):
		conf = Settings.get()
		conf.set_string('/modules', 'test_external_scripts', 'CheckExternalScripts')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		
		conf.set_string('/settings/pytest/scripts', 'test_external_script', 'test_external_script.py')
		
		conf.set_string('/settings/test_external_scripts', 'allow arguments', 'false')
		conf.set_string('/settings/test_external_scripts', 'allow nasty characters', 'false')

		conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_ok', 	'scripts\\check_ok.bat')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_long', 	'scripts\\check_long.bat')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_bat', 	'scripts\\check_test.bat')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_ps1', 	'cmd /c echo scripts\\check_test.ps1; exit($lastexitcode) | powershell.exe -command -')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_vbs', 	'cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs scripts\\check_test.vbs')

		conf.set_string('/settings/test_external_scripts/scripts', 'tes_sa_bat', 	'scripts\\check_test.bat "ARG1" "ARG 2" "A R G 3"')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_sa_ps1', 	'scripts\\check_test.ps1 "ARG1" "ARG 2" "A R G 3"')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_sa_vbs', 	'scripts\\check_test.vbs "ARG1" "ARG 2" "A R G 3"')

		conf.set_string('/settings/test_external_scripts/scripts', 'tes_sca_bat', 	'scripts\\check_test.bat $ARG1$ "$ARG2$" "$ARG3$"')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_sca_ps1', 	'scripts\\check_test.ps1 $ARG1$ "$ARG2$" "$ARG3$"')
		conf.set_string('/settings/test_external_scripts/scripts', 'tes_sca_vbs', 	'scripts\\check_test.vbs $ARG1$ "$ARG2$" "$ARG3$"')
		
		conf.set_string('/settings/test_external_scripts/wrapped scripts', 'tes_ws_bat', 	'check_test.bat')
		conf.set_string('/settings/test_external_scripts/wrapped scripts', 'tes_ws_ps1', 	'check_test.ps1')
		conf.set_string('/settings/test_external_scripts/wrapped scripts', 'tes_ws_vbs', 	'check_test.vbs')
		
		conf.set_string('/settings/test_external_scripts/wrappings', 'vbs', 'cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%')
		conf.set_string('/settings/test_external_scripts/wrappings', 'ps1', 'cmd /c echo scripts\\\\%SCRIPT% %ARGS%; exit($lastexitcode) | powershell.exe -command -')
		conf.set_string('/settings/test_external_scripts/wrappings', 'bat', 'scripts\\\\%SCRIPT% %ARGS%')

		conf.set_string('/settings/test_external_scripts/alias', 'tes_alias_ok', 	'tes_script_bat')
		conf.set_string('/settings/test_external_scripts/alias', 'tes_aa_ok', 	'tes_script_bat  "ARG1" "ARG 2" "A R G 3"')
		
		conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		None

	def shutdown(self):
		None

	def require_boot(self):
		return True

setup_singleton(ExternalScriptTest)

all_tests = [ExternalScriptTest]

def __main__():
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
