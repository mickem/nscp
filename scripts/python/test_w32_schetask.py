from NSCP import Settings, Registry, Core, log, log_debug, status, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
import random
import subprocess
import uuid
import os
import sys, stat, datetime, time
from subprocess import check_output

class Win32SchedTaskTest(BasicTest):

    reg = None
    conf = None
    core = None

    def desc(self):
        return 'Testcase for w32 check_wmi module'

    def title(self):
        return 'Win32File tests'

    def setup(self, plugin_id, prefix):
        self.reg = Registry.get(plugin_id)

    def teardown(self):
        None
    
    def get_count(self,perf):
        if not perf:
            return -1
        (title, data) = perf.split('=')
        if not data:
            return -1
        (count, warn, crit) = data.split(';')
        return int(count)

    def check_ts_query(self, task, code):
        result = TestResult('Checking task %s'%task)
        for i in [0, 1, 2, 3, 4]:
            # check_tasksched "filter=title = 'NSCPSample_CRIT'" "warn=exit_code != 3"
            args = ["filter=title = 'NSCPSample_%s'"%task, 
                "warn=exit_code = %d"%i]
            log_debug(', '.join(args))
            (ret, msg, perf) = self.core.simple_query('check_tasksched', args)
            
            if i == code:
                result.assert_equals(ret, status.WARNING, 'Verify WARN result: %s'%msg)
            else:
                result.assert_equals(ret, status.OK, 'Verify OK result: %s'%msg)
            
        return result
        
    def run_test(self):
        result = TestResult('Testing W32 task scheduler')
        for (state, code) in [('OK', 0), ('WARN', 1), ('CRIT', 2), ('LONG', 0)]:
            result.add(self.check_ts_query(state, code))
        return result

    def install(self, arguments):
        t = datetime.datetime.fromtimestamp(time.mktime(time.localtime()))
        t = t + datetime.timedelta(seconds=60)
        tm = time.strftime("%H:%M", t.timetuple())
        folder = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        log("Adding scheduled tasks")
        for state in ['OK', 'WARN', 'CRIT', 'LONG']:
            cmd = "SchTasks /Create /SC DAILY /TN NSCPSample_%s /TR \"%s\\check_test.bat %s\" /ST %s /F"%(state, folder, state, tm)
            log_debug(cmd)
            check_output(cmd)
        log("Waiting 1 minute (for tasks to run)")
        time.sleep(60)
        
        conf = self.conf
        conf.set_string('/modules', 'test_tsch', 'CheckTaskSched')
        conf.set_string('/modules', 'pytest', 'PythonScript')
        conf.set_string('/settings/pytest/scripts', 'test_w32_tsch', __file__)
        conf.save()

    def uninstall(self):
        log("Removing scheduled tasks")
        for state in ['OK', 'WARN', 'CRIT', 'LONG']:
            log_debug("SchTasks /Delete /TN NSCPSample_%s /F"%state)
            check_output("SchTasks /Delete /TN NSCPSample_%s /F"%state)

    def help(self):
        None

    def init(self, plugin_id, prefix):
        self.reg = Registry.get(plugin_id)
        self.core = Core.get(plugin_id)
        self.conf = Settings.get(plugin_id)

    def shutdown(self):
        None

setup_singleton(Win32SchedTaskTest)

all_tests = [Win32SchedTaskTest]

def __main__(args):
    install_testcases(all_tests)
    
def init(plugin_id, plugin_alias, script_alias):
    init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
    log("shutdown")
    shutdown_testcases()
