from NSCP import Settings, Registry, Core, log, log_error, status

class Callable:
	def __init__(self, anycallable):
		self.__call__ = anycallable

class DummyTest:
	instance = None
	reg = None
	plugin_id = ''
	
	class SingletonHelper:
		def __call__( self, *args, **kw ) :
			if DummyTest.instance is None :
				object = DummyTest()
				DummyTest.instance = object
			return DummyTest.instance

	getInstance = SingletonHelper()

	def desc(self):
		return 'Dummy test class'

	def dummy_static_function(foo):
		instance = ChannelTest.getInstance()
	dummy_static_function = Callable(dummy_static_function)
		
	def setup(self, plugin_id, prefix):
		self.plugin_id = plugin_id
		None
		
	def teardown(self):
		None

	def run_test(self):
		fail_count = 4
		if fail_count > 0:
			log("ERROR: %d tests failed"%fail_count)
		else:
			log("OK: all tests successfull")
		return (fail_count, 9)

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

	def add_entry(self, e):
		self.results.append(e)
	
	def add(self, result):
		for e in result.results:
			e.indent()
		self.results.extend(result.results)

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
			log("ERROR: %d/%d test(s) failed"%(count-okcount, count))
		return self
		
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

		
def log_result(result):
	if result.fail_count > 0:
		log("ERROR: %d/%dtests failed"%(result.fail_count,result.count))
	else:
		log("OK: all %d tests successfull"%result.count)
	return (result.fail_count, result.count)

	
def run_test(plugin_id, prefix, cls):
	result = TestResult()
	instance = cls.getInstance()
	instance.setup(plugin_id, prefix)
	result.add(instance.run_test())
	instance.teardown()
	return result

def run_tests(plugin_id, prefix, list):
	result = TestResult()
	for c in list:
		result.add(run_test(plugin_id, prefix, c))
	return result

def test_usage_sample(arguments):
	global prefix
	global plugin_id

	(all_failed, all_count) = run_tests([DummyTest])
	if all_failed == 0:
		return (status.OK, 'All tests ok: %d'%all_count)
	else:
		return (status.CRITICAL, 'Tests failed %d of %d'%(all_failed, all_count))
