from NSCP import Settings, Registry, Core, log, log_error, status
import os
import inspect

test_manager = None

def install_testcases(tests):
	test_manager = create_test_manager()
	test_manager.add(tests)
	test_manager.install()

def init_testcases(plugin_id, plugin_alias, script_alias, tests):
	test_manager = create_test_manager(plugin_id, plugin_alias, script_alias)
	test_manager.add(tests)
	test_manager.init()

def shutdown_testcases():
	get_test_manager().shutdown()

def get_test_manager():
	global test_manager
	return test_manager

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
	log('Setting path: %s'%src)
	if not src:
		cf = inspect.currentframe()
		if cf:
			bf = cf.f_back
			if bf:
				src = bf.f_code.co_filename
	klass.__source__ = src
	log('==>%s'%src)

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
		result = TestResult()
		result.add_message(False, 'TODO add implementation')
		return result

	def install(self, arguments):
		conf = Settings.get()
		conf.set_string('/modules', 'pytest', 'PythonScript')
		log('==> %s'%self.__source__)
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
		
		
class TestResult:
	class Entry:
		status = False
		desc = 'Unassigned result'
		level = 0
		error = None
		def __init__(self, status, desc, error):
			self.status = status
			self.desc = desc
			self.error = error

		def log(self):
			if self.status:
				log('%s%s'%(''.rjust(self.level, ' '), self))
			else:
				log_error('%s%s'%(''.rjust(self.level, ' '), self))
		
		def is_ok(self):
			return self.status
	
		def indent(self):
			self.level = self.level + 1
			
		def __str__(self):
			if self.status:
				return 'OK: %s'%self.desc
			else:
				return 'ERROR: %s (%s)'%(self.desc, self.error)

	results = []
	
	def __init__(self):
		self.results = []

	def add_message(self, status, message, error = None):
		e = TestResult.Entry(status, message, error)
		e.log()
		self.add_entry(e)
		
	def assert_equals(self, s1, s2, msg):
		self.add_message(s1 == s2, msg, '"%s" != "%s"'%(s1, s2))
		
	def assert_contains(self, s1, s2, msg):
		if s1 == s2:
			self.add_message(s1 in s2 or s2 in s1, msg, '"%s" (contains) "%s"'%(s1, s2))
		elif s1 == None or s2 == None:
			self.add_message(False, msg, '"%s" (contains) "%s"'%(s1, s2))
		else:
			self.add_message(s1 in s2 or s2 in s1, msg, '"%s" (contains) "%s"'%(s1, s2))
		

	def add_entry(self, e):
		self.results.append(e)
	
	def add(self, result):
		try:
			for e in result.results:
				e.indent()
			self.results.extend(result.results)
		except:
			log_error('Failed to process results...')

	def log(self):
		okcount = 0
		count = len(self.results)
		for e in self.results:
			e.log()
			if e.is_ok():
				okcount = okcount + 1
		if okcount == count:
			log("OK: %d test(s) successfull"%count)
		else:
			log("ERROR: %d of %d test(s) succedded (%d failed)"%(okcount, count, count-okcount))
		return self
		
	def is_ok(self):
		for e in self.results:
			if not e.is_ok():
				return False
		return True

	def __str__(self):
		s = ''
		for e in self.results:
			s += '%s, '%e
		return s

	def return_nagios(self):
		okcount = 0
		count = len(self.results)
		for e in self.results:
			if e.is_ok():
				okcount = okcount + 1
		self.log()
		if okcount == count:
			return (status.OK, "OK: %d test(s) successfull"%count)
		else:
			return (status.CRITICAL, "ERROR: %d/%d test(s) failed"%(count-okcount, count))

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
	
	def add(self, suite):
		if isinstance(suite, (list)):
			for s in suite:
				self.suites.append(s)
		else:
			self.suites.append(suites)

	def run_suite(self, suite):
		result = TestResult()
		for c in list:
			result.add(run_test(plugin_id, prefix, c))
		return result
		
	def run(self, arguments = []):
		result = TestResult()
		for suite in self.suites:
			instance = suite.getInstance()
			instance.setup(self.plugin_id, self.prefix)
			tmp = TestResult()
			tmp.add(instance.run_test())
			result.add_message(tmp.is_ok(), 'Running suite: %s'%instance.title())
			result.add(tmp)
			instance.teardown()
		return result

	def init(self):
		for suite in self.suites:
			instance = suite.getInstance()
			instance.init(self.plugin_id)
			
	def install(self, arguments = []):
		boot = False
		for suite in self.suites:
			instance = suite.getInstance()
			instance.install(arguments)
			if instance.require_boot():
				boot = True

		#core = Core.get()
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
			instance.shutdown()
			
		
