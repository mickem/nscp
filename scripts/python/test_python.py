from NSCP import Settings, Registry, Core, log, status, log_error, log_debug, sleep
from test_helper import BasicTest, TestResult, install_testcases, init_testcases, shutdown_testcases
from time import time

install_checks = 1000
time_to_run = 60


# ---------------------------------------------------------------------------
# Performance-data round-trip
#
# Each case is (handler_name, perf_in, expected_perf_out). A handler that
# returns `perf_in` is registered via Registry.simple_function. When the test
# calls core.simple_query(handler_name), the runtime path is:
#
#   Python handler returns (status, msg, perf_in)
#     -> PythonScript.cpp parses perf_in into the protobuf line via
#        nscapi::protobuf::functions::parse_performance_data
#     -> the response is serialised
#     -> core.simple_query reads it back, calling
#        nscapi::protobuf::functions::build_performance_data to produce the
#        perf string returned to Python.
#
# So perf_out == expected_perf_out asserts the parse+format pipeline is
# faithful for that input. Issue #748 (Nagios range syntax in
# warning/critical) is the headline case for the new format; legacy numeric
# thresholds also covered here so we have a regression net for the existing
# behaviour.
#
# Notes on expected output:
#   - The formatter always quotes the alias, so we use 'name' on both sides
#     for exact round-trip.
#   - The formatter elides trailing empty `;` separators, so we don't put
#     them on the expected side either.
# ---------------------------------------------------------------------------
PERF_ROUNDTRIP_CASES = [
    # --- legacy numeric thresholds ---
    ('py_perf_plain',          "'load'=42",                   "'load'=42"),
    ('py_perf_with_unit',      "'mem'=50%",                   "'mem'=50%"),
    ('py_perf_value_warn',     "'x'=10;5",                    "'x'=10;5"),
    ('py_perf_full_numeric',   "'cpu'=50%;70;90;0;100",       "'cpu'=50%;70;90;0;100"),
    ('py_perf_multi_metric',   "'a'=1 'b'=2;5",               "'a'=1 'b'=2;5"),

    # --- new range syntax (issue #748) ---
    # The headline case from the bug report. Pre-fix this came back as
    # 'FOO'=10;4;6 because trim_to_double truncated at the colon.
    ('py_perf_range_basic',    "'FOO'=10;4:5;6:9",            "'FOO'=10;4:5;6:9"),
    # Open lower / upper bounds.
    ('py_perf_range_open',     "'x'=1;10:;:20",               "'x'=1;10:;:20"),
    # Inverted-range and infinity-prefix forms.
    ('py_perf_range_inverted', "'x'=1;@10:20;~:30",           "'x'=1;@10:20;~:30"),
    # Range with UOM on the value itself.
    ('py_perf_range_unit',     "'x'=5s;4:6;7:8",              "'x'=5s;4:6;7:8"),
    # Mixed: plain-numeric warning, range critical (or vice versa).
    ('py_perf_range_mixed',    "'x'=10;5;6:9",                "'x'=10;5;6:9"),
    # Range thresholds plus min/max (min/max are spec'd as single values).
    ('py_perf_range_minmax',   "'x'=50%;@10:90;:95;0;100",    "'x'=50%;@10:90;:95;0;100"),
]


def _make_perf_handler(perf_string):
    """Build a (status, msg, perf) callback bound to a fixed perf string.

    Each registered handler closes over its own perf payload so the test
    can assert the round-tripped value without having to thread the input
    through arguments.
    """
    def handler(_arguments):
        return status.OK, 'perf round-trip', perf_string
    return handler

class PythonTest(BasicTest):

	noop_count = 0
	stress_count = 0
	
	key = ''
	reg = None
	conf = None
	core = None

	@staticmethod
	def noop_handler(arguments):
		global instance
		instance.noop_count = instance.noop_count + 1
		return status.OK, 'Got call %d'%instance.noop_count, ''

	
	@staticmethod
	def stress_handler(channel, source, command, code, message, perf):
		global instance
		instance.stress_count = instance.stress_count + 1
		log_debug('Got message %d/%d on %s'%(instance.stress_count, instance.noop_count, channel))
	
	def desc(self):
		return 'Testcase for python script module'

	def title(self):
		return 'PythonScript tests'

	def setup(self, plugin_id, prefix):
		log('Loading Python unit tests')
		self.key = '_%stest_command'%prefix
		self.reg.simple_function('py_stress_noop', PythonTest.noop_handler, 'This is a simple noop command')
		self.reg.simple_subscription('py_stress_test', PythonTest.stress_handler)
		self.conf.set_string('/settings/test_scheduler', 'threads', '50')
		self.core.reload('test_scheduler')
		

	def teardown(self):
		self.conf.set_string('/settings/test_scheduler', 'threads', '0')
		self.core.reload('test_scheduler')
		None

	def run_test(self, cases = None):
		result = TestResult()
		start = time()
		total_count = install_checks*time_to_run/5
		while self.stress_count < total_count:
			log('Waiting for %d: %d/%d'%(total_count, self.stress_count, self.noop_count))
			old_stress_count = self.stress_count
			old_noop_count = self.noop_count
			sleep(5000)
			result.add_message(True, 'Commands/second: %d/%d'%( (self.stress_count-old_stress_count)/5, (self.noop_count-old_noop_count)/5 ) )
			log(f'Commands/second: {(self.stress_count-old_stress_count)/5}/{(self.noop_count-old_noop_count)/5}')
		elapsed = (time() - start)
		if elapsed == 0:
			elapsed = 1
		result.add_message(True, 'Summary Collected %d instance in %d seconds: %d/s'%(self.stress_count, elapsed, self.stress_count/elapsed))
		return result

	def install(self):
		self.conf.set_string('/modules', 'test_scheduler', 'Scheduler')
		self.conf.set_string('/modules', 'pytest', 'PythonScript')

		self.conf.set_string('/settings/pytest/scripts', 'test_python', 'test_python.py')
		
		base_path = '/settings/test_scheduler'
		self.conf.set_string(base_path, 'threads', '0')

		default_path = f'{base_path}/schedules/default'
		self.conf.set_string(default_path, 'channel', 'py_stress_test')
		#self.conf.set_string(default_path, 'alias', 'stress')
		self.conf.set_string(default_path, 'command', 'py_stress_noop')
		self.conf.set_string(default_path, 'interval', '5s')
		for i in range(1, install_checks):
			alias = 'stress_python_%i'%i
			self.conf.set_string(f'{base_path}/schedules', alias, 'py_stress_noop')

		self.conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		self.key = '_test_command'
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

		None

	def shutdown(self):
		None

class PerfDataTest(BasicTest):
	"""End-to-end round-trip for performance data through PythonScript.

	A Python handler returns (status, msg, perf_string); PythonScript parses
	that string into the protobuf line via nscapi::protobuf::functions::
	parse_performance_data, and the simple_query response is serialised back
	via build_performance_data. So this test exercises the same parser +
	formatter pair that handles external-script stdout. Issue #748 added
	support for Nagios range syntax (e.g. `'FOO'=10;4:5;6:9`) - prior to
	that fix the colon and everything after it was silently truncated.

	Cheap to run: registers a handful of query handlers up front and then
	invokes each one once. No scheduler, no boot, no stress workload, so
	it sits alongside the heavier PythonTest stress suite without slowing
	the common case.
	"""

	reg = None
	conf = None
	core = None

	def desc(self):
		return 'Round-trip performance data through PythonScript (parse + format pipeline)'

	def title(self):
		return 'PythonScript perfdata round-trip'

	def setup(self, plugin_id, prefix):
		# Each case gets its own registered query handler closing over the
		# perf payload it should emit; the test then asks core to invoke
		# the handler and asserts the formatter rebuilt the same string.
		for (name, perf_in, _expected) in PERF_ROUNDTRIP_CASES:
			self.reg.simple_function(name, _make_perf_handler(perf_in),
			                          'Emit perf data for round-trip test: %s' % name)

	def teardown(self):
		None

	def run_test(self, cases=None):
		suite = TestResult('Perfdata round-trip')
		for (name, perf_in, expected) in PERF_ROUNDTRIP_CASES:
			(rc, msg, perf_out) = self.core.simple_query(name, [])
			suite.assert_equals(rc, status.OK, '%s: status OK' % name)
			suite.assert_equals(perf_out, expected,
			                    '%s: round-trip "%s" -> "%s"' % (name, perf_in, perf_out))
		return suite

	def install(self):
		self.conf.set_string('/modules', 'pytest', 'PythonScript')
		self.conf.set_string('/settings/pytest/scripts', 'test_python', 'test_python.py')
		self.conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id):
		self.reg = Registry.get(plugin_id)
		self.conf = Settings.get(plugin_id)
		self.core = Core.get(plugin_id)

	def shutdown(self):
		None


instance = PythonTest()
perf_instance = PerfDataTest()
all_tests = [instance, perf_instance]

def __main__(args):
	install_testcases(all_tests)

def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()

