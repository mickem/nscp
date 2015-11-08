from NSCP import Settings, Registry, Core, log, status, log_error, sleep
import sys, difflib
import os

from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
import plugin_pb2
from types import *
import socket
import uuid
import unicodedata

import threading
sync = threading.RLock()


LONG_OUTPUT = """Test arguments are: (LONG "$ARG2$" "$ARG3$")\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n
0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"""


class ExternalScriptTest(BasicTest):
	instance = None
	key = ''
	reg = None
	conf = None
	core = None
	_responses = {}
	_requests = {}
	
	def setup(self, plugin_id, prefix):
		self.key = '_%stest_command'%prefix
		self.reg = Registry.get(plugin_id)

	def teardown(self):
		None
		
	def do_one_test(self, script, expected = status.OK, message = "Foo Bar", args=[], cleanup=True):
		result = TestResult('%s (%s)'%(script, args))
		(ret, msg, perf) = self.core.simple_query(script, args)
		if cleanup and os.name != 'nt':
			message = message.replace('"', '')
			message = message.replace('$ARG1$', '$')
			message = message.replace('$ARG2$', '$')
			message = message.replace('$ARG3$', '$')

		message = message.replace('\r', '\n')
		message = message.replace('\n\n', '\n')
		msg = msg.replace('\r', '\n')
		msg = msg.replace('\n\n', '\n')

		result.assert_equals(ret, expected, 'Validate return code for %s'%script)
		result.assert_equals(msg, message, 'Validate return message for %s'%script)
		if msg != message:
			diff = difflib.ndiff(msg.splitlines(1), message.splitlines(1))
			for l in diff:
				log_error(l)
		return result

	def run_test(self):
		ret = TestResult('External scripts test suite')
		result = TestResult('Arguments NOT allowed')
		result.add(self.do_one_test('tes_script_ok', message='OK: Everything is going to be fine'))
		result.add(self.do_one_test('tes_script_test', message='Test arguments are: (  )'))
		
		result.add(self.do_one_test('tes_sa_test', status.OK, 'Test arguments are: ("ARG1" "ARG 2" "A R G 3")'))

		
		result.add(self.do_one_test('tes_script_test', status.UNKNOWN, 'Arguments not allowed see nsclient.log for details', ['NOT ALLOWED']))
		
		ret.add(result)
		self.conf.set_string('/settings/test_external_scripts', 'allow arguments', 'true')
		self.core.reload('test_external_scripts')

		result = TestResult('Arguments allowed')
		
		if os.name == 'nt':
			tests = ['bat', 'ps1']
		else:
			tests = ['sh']
		for t in tests:
			script = 'tes_sca_%s'%t
			subresult = TestResult(t)
			subresult.add(self.do_one_test(script, status.OK, 'Test arguments are: (OK "$ARG2$" "$ARG3$")', ['OK']))
			subresult.add(self.do_one_test(script, status.WARNING, 'Test arguments are: (WARN "$ARG2$" "$ARG3$")', ['WARN']))
			subresult.add(self.do_one_test(script, status.CRITICAL, 'Test arguments are: (CRIT "$ARG2$" "$ARG3$")', ['CRIT']))
			subresult.add(self.do_one_test(script, status.UNKNOWN, 'Test arguments are: (UNKNOWN "$ARG2$" "$ARG3$")', ['UNKNOWN']))
			subresult.add(self.do_one_test(script, status.OK, 'Test arguments are: (OK "String with space" "A long long option with many spaces")', ['OK', 'String with space', 'A long long option with many spaces']))
			subresult.add(self.do_one_test(script, status.OK, LONG_OUTPUT, ['LONG']))

			subresult.add(self.do_one_test(script, status.UNKNOWN, 'Request contained illegal characters set /settings/external scripts/allow nasty characters=true!', ['OK', '$$$ \\ \\', '$$$ \\ \\']))

			result.add(subresult)

		subresult = TestResult('Upper and lower case')
		subresult.add(self.do_one_test("tes_upper_LOWER", status.OK, 'OK: Everything is going to be fine', ['OK']))
		subresult.add(self.do_one_test("alias_UPPER_lower", status.OK, 'OK: Everything is going to be fine', ['OK']))

		result.add(subresult)

		ret.add(result)
			
		self.conf.set_string('/settings/test_external_scripts', 'allow nasty characters', 'true')
		self.core.reload('test_external_scripts')

		result = TestResult('Nasty Arguments allowed')
		for t in tests:
			script = 'tes_sca_%s'%t
			subresult = TestResult(t)
			if os.name == 'nt':
				subresult.add(self.do_one_test(script, status.OK, 'Test arguments are: (OK "$$$ \\ \\" "$$$ \\ \\")', ['OK', '$$$ \\ \\', '$$$ \\ \\'], False))
			else:
				subresult.add(self.do_one_test(script, status.OK, 'Test arguments are: (OK $ \\ " $  ")', ['OK', '$ \\ \\', '$ \\ \\'], False))
			result.add(subresult)
		
		ret.add(result)
		return ret
		
	def install(self, arguments):
		self.conf.set_string('/modules', 'test_external_scripts', 'CheckExternalScripts')
		self.conf.set_string('/modules', 'pytest', 'PythonScript')
		
		self.conf.set_string('/settings/pytest/scripts', 'test_external_script', 'test_external_script.py')
		
		self.conf.set_string('/settings/test_external_scripts', 'allow arguments', 'false')
		self.conf.set_string('/settings/test_external_scripts', 'allow nasty characters', 'false')

		if os.name == 'nt':
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_UPPER_lower', 	'scripts\\check_ok.bat')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_ok', 	'scripts\\check_ok.bat')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_long', 	'scripts\\check_long.bat')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_test', 	'scripts\\check_test.bat')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_bat', 	'scripts\\check_test.bat')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_ps1', 	'cmd /c echo scripts\\check_test.ps1; exit($lastexitcode) | powershell.exe -command -')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_vbs', 	'cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs scripts\\check_test.vbs')

			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sa_test', 	'scripts\\check_test.bat "ARG1" "ARG 2" "A R G 3"')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sa_ps1', 	'scripts\\check_test.ps1 "ARG1" "ARG 2" "A R G 3"')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sa_vbs', 	'scripts\\check_test.vbs "ARG1" "ARG 2" "A R G 3"')

			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sca_bat', 	'scripts\\check_test.bat $ARG1$ "$ARG2$" "$ARG3$"')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sca_ps1', 	'cmd /c echo scripts\\check_test.ps1 \'$ARG1$\' \'$ARG2$\' \'$ARG3$\'; exit($lastexitcode) | powershell.exe -command -')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sca_vbs', 	'scripts\\check_test.vbs $ARG1$ "$ARG2$" "$ARG3$"')
			
			self.conf.set_string('/settings/test_external_scripts/wrapped scripts', 'tes_ws_bat', 	'check_test.bat')
			self.conf.set_string('/settings/test_external_scripts/wrapped scripts', 'tes_ws_ps1', 	'check_test.ps1')
			self.conf.set_string('/settings/test_external_scripts/wrapped scripts', 'tes_ws_vbs', 	'check_test.vbs')
			
			self.conf.set_string('/settings/test_external_scripts/wrappings', 'vbs', 'cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%')
			self.conf.set_string('/settings/test_external_scripts/wrappings', 'ps1', 'cmd /c echo scripts\\\\%SCRIPT% %ARGS%; exit($lastexitcode) | powershell.exe -command -')
			self.conf.set_string('/settings/test_external_scripts/wrappings', 'bat', 'scripts\\\\%SCRIPT% %ARGS%')

		else:
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_UPPER_lower', 	'scripts/check_ok.sh')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_ok', 	'scripts/check_ok.sh')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_long', 	'scripts/check_long.sh')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_sh', 	'scripts/check_test.sh')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_script_test', 	'scripts/check_test.sh')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sa_test', 		'scripts/check_test.sh "ARG1" "ARG 2" "A R G 3"')
			self.conf.set_string('/settings/test_external_scripts/scripts', 'tes_sca_sh', 	'scripts/check_test.sh $ARG1$ "$ARG2$" "$ARG3$"')

		self.conf.set_string('/settings/test_external_scripts/alias', 'tes_alias_ok', 'tes_script_test')
		self.conf.set_string('/settings/test_external_scripts/alias', 'tes_aa_ok', 'tes_script_test  "ARG1" "ARG 2" "A R G 3"')
		self.conf.set_string('/settings/test_external_scripts/alias', 'alias_UPPER_lower', 'tes_UPPER_lower')

		self.conf.save()

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

	def require_boot(self):
		return True

setup_singleton(ExternalScriptTest)

all_tests = [ExternalScriptTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
