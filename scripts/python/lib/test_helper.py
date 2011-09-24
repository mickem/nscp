from NSCP import Settings, Registry, Core, log, status

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

def run_test(plugin_id, prefix, cls):
	instance = cls.getInstance()
	instance.setup(plugin_id, prefix)
	ret = instance.run_test()
	instance.teardown()
	log('Tested %s (%s of %s)'%(instance.desc(), ret[0], ret[1]))
	return ret

class TestResult:
	fail_count = 0
	count = 0
	
	def __init__(self):
		None

	def add_message(self, status, message):
		if status:
			log('OK: %s'%message)
			self.add(0, 1)
		else:
			log('TEST FAILED: %s'%message)
			self.add(1, 1)
	
	def add(self, failed, total = None):
		if total == None:
			(failed, total) = failed
		self.fail_count += failed
		self.count += total

	def log(self):
		if self.fail_count > 0:
			log("ERROR: %d/%d test(s) failed"%(self.fail_count,self.count))
		else:
			log("OK: %d test(s) successfull"%self.count)
		return (self.fail_count, self.count)
		
	def return_nagios(self):
		self.log()
		if self.fail_count == 0:
			return (status.OK, 'All tests ok: %d'%self.count)
		else:
			return (status.CRITICAL, 'Tests failed %d of %d'%(self.fail_count, self.count))
		
	
def log_test_result(status, message):
	if status:
		log('OK: %s'%message)
		return (0, 1)
	else:
		log('TEST FAILED: %s'%message)
		return (1, 1)
		
def log_result(result):
	if result.fail_count > 0:
		log("ERROR: %d/%dtests failed"%(result.fail_count,result.count))
	else:
		log("OK: all %d tests successfull"%result.count)
	return (result.fail_count, result.count)

def run_tests(plugin_id, prefix, list):
	all_failed = 0
	all_count = 0
	for c in list:
		(failed, count) = run_test(plugin_id, prefix, c)
		all_failed += failed
		all_count += count
	return (all_failed, all_count)

def test_usage_sample(arguments):
	global prefix
	global plugin_id

	(all_failed, all_count) = run_tests([DummyTest])
	if all_failed == 0:
		return (status.OK, 'All tests ok: %d'%all_count)
	else:
		return (status.CRITICAL, 'Tests failed %d of %d'%(all_failed, all_count))
