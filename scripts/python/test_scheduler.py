from NSCP import Settings, Registry, Core, log, status, log_debug, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
from time import time
import random
import os

prefix = 'scheduler'

class SchedulerTest(BasicTest):

    check_count = 0
    results_count = 0
    command_count = {}
    
    sched_alias = 'test_sched_%s'%prefix
    python_channel = 'test_sched_%s_py'%prefix
    command = 'test_sched_%s'%prefix
    sched_base_path = '/settings/%s'%sched_alias
    
    def simple_check_handler(arguments):
        instance = SchedulerTest.getInstance()
        return instance.wrapped_simple_check_handler(arguments)
    simple_check_handler = Callable(simple_check_handler)

    def wrapped_simple_check_handler(self, arguments):
        self.check_count = self.check_count + 1
        if arguments:
            if not arguments[0] in self.command_count:
                self.command_count[arguments[0]] = 1
            else:
                self.command_count[arguments[0]] = self.command_count[arguments[0]] + 1
            return (status.OK, arguments[0], '')
        return (status.OK, 'pong', '')

    def on_stress_handler(channel, source, command, code, message, perf):
        instance = SchedulerTest.getInstance()
        instance.wrapped_on_stress_handler(channel, source, command, code, message, perf)
    on_stress_handler = Callable(on_stress_handler)

    def wrapped_on_stress_handler(self, channel, source, command, code, message, perf):
        self.results_count = self.results_count + 1
        return None

    def desc(self):
        return 'Testcase for Scheduler'

    def title(self):
        return 'Test Scheduler'

    def setup(self, plugin_id, prefix):
        self.reg.simple_function(self.command, SchedulerTest.simple_check_handler, 'This is a simple noop command')
        self.reg.simple_subscription(self.python_channel, SchedulerTest.on_stress_handler)

        #self.core.reload('%s,delayed'%self.sched_alias)
        

    def teardown(self):
        self.conf.set_string(self.sched_base_path, 'threads', '0')
        self.core.reload(self.sched_alias)

    def check_one(self, result, key, min, max):
        result.assert_gt(self.command_count[key], min, 'check %s (%d) fired more then %d'%(key, self.command_count[key], min))
        result.assert_lt(self.command_count[key], max, 'check %s (%d) fired less then %d'%(key, self.command_count[key], max))
    
    def run_test(self):
        self.core.load_module('Scheduler', self.sched_alias)
        result = TestResult()
        start = time()

        last_major = 0
        elapsed = time()-start
        while elapsed < 60:
            if elapsed > 0:
                log("testing scheduler %d%% (collected %d instance in %d seconds)"%(elapsed/60*100, self.results_count, elapsed))
            sleep(2000)
            elapsed = time()-start
        result.add_message(True, 'Summary Collected %d instance in %d seconds: %d/s'%(self.results_count, elapsed, self.results_count/elapsed))
        self.check_one(result, "rand", 5, 10)
        self.check_one(result, "1s", 55, 65)
        self.check_one(result, "short", 10, 14)
        self.check_one(result, "30s", 1, 3)
        self.check_one(result, "explicit", 10, 14)
        self.check_one(result, "10s", 5, 7)

        return result

    def install(self, arguments):
        # Configure required modules
        self.conf.set_string('/modules', 'pytest', 'PythonScript')
        #self.conf.set_string('/modules', self.sched_alias, 'Scheduler')
        self.conf.set_string('/modules', 'CheckSystem', 'enabled')

        # Configure python
        self.conf.set_string('/settings/pytest/scripts', 'test_stress', 'test_scheduler.py')
        
        default_path = '%s/schedules/default'%self.sched_base_path
        self.conf.set_string(default_path, 'channel', self.python_channel)
        self.conf.set_string(default_path, 'command', "%s default"%self.command)
        self.conf.set_string(default_path, 'interval', '5s')
        self.conf.set_string(default_path, 'randomness', '0%')

        self.conf.set_string('%s/schedules'%(self.sched_base_path), 'python_checker_d', "%s short"%self.command)

        self.conf.set_string('%s/schedules/python_checker_e'%(self.sched_base_path), 'command', "%s explicit"%self.command)

        #self.conf.set_string('%s/schedules/python_checker_i'%(self.sched_base_path), 'interval', '1s')

        self.conf.set_string('%s/schedules/python_checker_1s'%(self.sched_base_path), 'command', "%s 1s"%self.command)
        self.conf.set_string('%s/schedules/python_checker_1s'%(self.sched_base_path), 'interval', '1s')

        self.conf.set_string('%s/schedules/python_checker_10s'%(self.sched_base_path), 'command', "%s 10s"%self.command)
        self.conf.set_string('%s/schedules/python_checker_10s'%(self.sched_base_path), 'interval', '10s')

        self.conf.set_string('%s/schedules/python_checker_30s'%(self.sched_base_path), 'command', "%s 30s"%self.command)
        self.conf.set_string('%s/schedules/python_checker_30s'%(self.sched_base_path), 'interval', '30s')

        self.conf.set_string('%s/schedules/python_checker_r10s'%(self.sched_base_path), 'command', "%s rand"%self.command)
        self.conf.set_string('%s/schedules/python_checker_r10s'%(self.sched_base_path), 'interval', '10s')
        self.conf.set_string('%s/schedules/python_checker_r10s'%(self.sched_base_path), 'randomness', '50%')

        self.conf.save()

    def uninstall(self):
        None

    def help(self):
        None

    def init(self, plugin_id, prefix):
        self.reg = Registry.get(plugin_id)
        self.conf = Settings.get(plugin_id)
        self.core = Core.get(plugin_id)
        None

    def shutdown(self):
        None

    def require_boot(self):
        return True

setup_singleton(SchedulerTest)

all_tests = [SchedulerTest]

def __main__(args):
    install_testcases(all_tests)
    
def init(plugin_id, plugin_alias, script_alias):
    init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
    shutdown_testcases()
