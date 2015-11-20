from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import os
import inspect

test_manager = None

def install_testcases(tests, args = []):
	test_manager = create_test_manager()
	test_manager.add(tests)
	test_manager.install()

def init_testcases(plugin_id, plugin_alias, script_alias, tests):
	test_manager = create_test_manager(plugin_id, plugin_alias, script_alias)
	test_manager.add(tests)
	test_manager.init()

def shutdown_testcases():
	if get_test_manager():
		get_test_manager().shutdown()
	destroy_test_manager()

def get_test_manager():
	global test_manager
	return test_manager

def destroy_test_manager():
	global test_manager
	if test_manager:
		test_manager.destroy()
	test_manager = None

def create_test_manager(plugin_id = 0, plugin_alias = '', script_alias = ''):
	global test_manager
	if not test_manager:
		test_manager = TestManager(plugin_id, plugin_alias, script_alias)
		
		reg = Registry.get(plugin_id)
		
		reg.simple_cmdline('help', display_help)
		reg.simple_cmdline('install_python_test', install_tests)
		reg.simple_cmdline('run_python_test', run_tests)

		reg.simple_function('py_unittest', run_tests, 'Run python unit test suite')
	
	return test_manager
	
def add_test_suite(suites):
	mgr = get_test_manager()
	if isinstance(suites, (list)):
		for s in suites:
			mgr.add(s)
	else:
		mgr.add(suites)
	
def install_tests(arguments = []):
	get_test_manager().install(arguments)
	return (status.OK, 'installed?')

def run_tests(arguments = []):
	result = get_test_manager().run(arguments)
	return result.return_nagios()

def display_help(arguments = []):
	return (status.OK, 'TODO')

class Callable:
	def __init__(self, anycallable):
		self.__call__ = anycallable

class SingletonHelper:
	klass = None
	def __init__(self, klass):
		self.klass = klass
	def __call__(self, *args, **kw):
		if not self.klass._instance:
			self.klass._instance = self.klass()
		return self.klass._instance

def setup_singleton(klass, src = None):
	klass.getInstance = SingletonHelper(klass)
	if not src:
		cf = inspect.currentframe()
		if cf:
			bf = cf.f_back
			if bf:
				src = bf.f_code.co_filename
	klass.__source__ = src

class BasicTest(object):

	_instance = None
	getInstance = None
	__source__ = ''

	def desc(self):
		return 'TODO: Describe: %s'%self.title()

	def title(self):
		return self._instance.__class__.__name__

	def setup(self, plugin_id, prefix):
		None

	def teardown(self):
		None

	def run_test(self):
		result = TestResult('run_test')
		result.add_message(False, 'TODO add implementation')
		return result

	def install(self, arguments):
		conf = Settings.get()
		conf.set_string('/modules', 'pytest', 'PythonScript')
		fn = os.path.basename(self.__source__)
		(sn, ext) = os.path.splitext(fn)
		conf.register_key('/settings/pytest/scripts', sn, 'string', 'UNIT TEST SCRIPT: %s'%self.title(), 'A script for running unittests for: %s'%self.desc(), fn)
		conf.set_string('/settings/pytest/scripts', sn, fn)
		
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
		return False
		
class TestResultEntry:
	status = False
	desc = 'Unassigned result'
	error = None
	def __init__(self, status, desc, error):
		self.status = status
		self.desc = desc
		self.error = error

	def log(self, prefix = '', indent = 0):
		if self.status:
			log_debug('%s%s%s'%(prefix, ''.rjust(indent, ' '), self))
		else:
			log_error('%s%s%s'%(prefix, ''.rjust(indent, ' '), self))
	
	def is_ok(self):
		return self.status

	def count(self):
		if self.status:
			return (1, 1)
		return (1, 0)

	def contains(self, other):
		if self == other:
			return True
		return False
		
	def __str__(self):
		if self.status:
			return 'OK: %s'%self.desc
		else:
			return 'ERROR: %s (%s)'%(self.desc, self.error)
		
class TestResultCollection(TestResultEntry):

	status = True
	title = None
	children = []
	def __init__(self, title, list = None):
		self.title = title
		self.children = []
		if list:
			self.extend(list)

	def log(self, prefix = '', indent = 0):
		start = '%s%s'%(prefix, ''.rjust(indent, ' '))
		if self.status:
			log_debug('%s%s'%(start, self))
		else:
			log_error('%s%s'%(start, self))
		for c in self.children:
			c.log(prefix, indent+1)
	
	def is_ok(self):
		return self.status

	def count(self):
		total_count = 0
		ok_count = 0
		#if self.status:
		#	ok_count = 1

		for c in self.children:
			(total, ok) = c.count()
			total_count = total_count + total
			ok_count = ok_count + ok

		return (total_count, ok_count)

	def contains(self, other):
		for c in self.children:
			if c.contains(other):
				return True
		return False

	def __str__(self):
		if self.status:
			return 'OK: %s'%self.title
		else:
			(total, ok) = self.count()
			return 'ERROR: %s (%d/%d)'%(self.title, ok, total)

	def extend(self, lst):
		if isinstance(lst, list):
			if self.status:
				for c in lst:
					if not c.is_ok():
						self.status = False
						
			for c in lst:
				if c.contains(self):
					log_error('Attempting to add a list with me in it')
					return
			self.children.extend(lst)
		else:
			self.append(lst)

	def append(self, entry):
		if not entry:
			log_error('Attempting to add invalid entry (None)')
		elif entry == self:
			log_error('Attempting to add self to self')
		else:
			if self.status and not entry.is_ok():
				self.status = False
			self.children.append(entry)

class ArgumentParserError(Exception): pass

import argparse

class ThrowingArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise ArgumentParserError(message)

class TestResult(TestResultCollection):

	def __init__(self, title = 'DUMMY TITLE'):
		TestResultCollection.__init__(self, title)

	def add_message(self, status, message, error = None):
		e = TestResultEntry(status, message, error)
		e.log()
		self.append(e)
		
	def assert_equals(self, s1, s2, msg):
		self.add_message(s1 == s2, msg, '"%s" != "%s"'%(s1, s2))
		
	def assert_contains(self, s1, s2, msg):
		if s1 == s2:
			self.add_message(s1 in s2 or s2 in s1, msg, '"%s" (contains) "%s"'%(s1, s2))
		elif s1 == None or s2 == None:
			self.add_message(False, msg, '"%s" (contains) "%s"'%(s1, s2))
		else:
			self.add_message(s1 in s2 or s2 in s1, msg, '"%s" (contains) "%s"'%(s1, s2))

	def assert_not_contains(self, s1, s2, msg):
		if s1 == s2:
			self.add_message(False, msg, '"%s" (equals) "%s"'%(s1, s2))
		elif s1 == None or s2 == None:
			self.add_message(True, msg, '"%s" (is null?) "%s"'%(s1, s2))
		else:
			self.add_message(not (s1 in s2 or s2 in s1), msg, '"%s" (does not contains) "%s"'%(s1, s2))

	def add_entry(self, e):
		self.append(e)

	def add(self, result):
		self.extend(result)

	def return_nagios(self):
		(total, ok) = self.count()
		log_debug(' | Test result log (only summary will be returned to query)')
		self.log(' | ')
		if total == ok:
			return (status.OK, "OK: %d test(s) successfull"%(total))
		else:
			return (status.CRITICAL, "ERROR: %d/%d test(s) failed"%(total-ok, total))

class TestManager:
	
	suites = []
	prefix = ''
	plugin_id = None
	plugin_alias = None
	script_alias = None
	
	def __init__(self, plugin_id = 0, plugin_alias = '', script_alias = ''):
		if script_alias:
			self.prefix = '%s_'%script_alias
		self.plugin_id = plugin_id
		self.plugin_alias = plugin_alias
		self.script_alias = script_alias
		self.suites = []
	
	def add(self, suite):
		if isinstance(suite, list):
			for s in suite:
				self.add(s)
		else:
			if not suite in self.suites:
				self.suites.append(suite)

	def run_suite(self, suite):
		result = TestResult('Running suite: %s'%suite.title())
		for c in list:
			result.add(run_test(plugin_id, prefix, c))
		return result
		
	def run(self, arguments = []):
		cases = []
		try:
			parser = ThrowingArgumentParser(prog='nscp')
			parser.add_argument("--script", help="The script to run (sort of ignored)", action='store')
			parser.add_argument("--case", help="Which test case to run", action='append')
			args = parser.parse_args(arguments)
			cases = args.case
		except Exception as e:
			log_error('Failed to parse command line: %s'%e)
			
		result = TestResult('Test result for %d suites'%len(self.suites))
		for suite in self.suites:
			instance = suite.getInstance()
			instance.setup(self.plugin_id, self.prefix)
			suite_result = TestResult('Running suite: %s'%instance.title())
			if cases:
				suite_result.append(instance.run_test(cases))
			else:
				suite_result.append(instance.run_test())
			result.append(suite_result)
			result.add_message(suite_result.is_ok(), 'Result from suite: %s'%instance.title())
			instance.teardown()
		return result

	def init(self):
		for suite in self.suites:
			instance = suite.getInstance()
			instance.init(self.plugin_id, self.prefix)
			
	def destroy(self):
		self.suites = []
		self.prefix = ''
		self.plugin_id = None
		self.plugin_alias = None
		self.script_alias = None
			
	def install(self, arguments = []):
		boot = False
		for suite in self.suites:
			instance = suite.getInstance()
			instance.install(arguments)
			if instance.require_boot():
				boot = True

		#core = Core.get()
		#core.reload('service')
		#(code, msg, perf) = core.simple_query('py_unittest', [])
		
		log('-+---==(TEST INSTALLER)==---------------------------------------------------+-')
		log(' | Setup nessecary configuration for running test                           |')
		log(' | This includes: Loading the PythonScript module at startup                |')
		log(' | To use this please run nsclient++ in "test mode" like so:                |')
		if boot:
			log(' | nscp client --boot --query py_unittest                                   |')
		else:
			log(' | nscp client --query py_unittest                                          |')
		log('-+--------------------------------------------------------==(DAS ENDE!)==---+-')
			
	def shutdown(self):
		for suite in self.suites:
			instance = suite.getInstance()
			instance.uninstall()
		for suite in self.suites:
			instance = suite.getInstance()
			instance.shutdown()
			
		
