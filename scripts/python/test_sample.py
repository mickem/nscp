from test_helper import BasicTest, TestResult, setup_singleton, install_testcases, init_testcases, shutdown_testcases

class SampleTest(BasicTest):
	pass

setup_singleton(SampleTest)

all_tests = [SampleTest]

def __main__():
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
