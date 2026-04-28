from NSCP import Settings, Registry, Core, log, log_debug, status, log_error, sleep
from test_helper import BasicTest, TestResult, install_testcases, init_testcases, shutdown_testcases
import os
import datetime, time
import subprocess

class Win32SchedTaskTest(BasicTest):

    reg = None
    conf = None
    core = None

    def __init__(self):
        self.plugin_id = None
        self.tasks = {}
        self.expected_exit_codes = {'OK': 0, 'WARN': 1, 'CRIT': 2, 'LONG': 0}

    def desc(self):
        return 'Testcase for w32 check_wmi module'

    def title(self):
        return 'Win32File tests'

    def _run_schtasks(self, args, ignore_failure=False):
        cmd = ['schtasks.exe'] + args
        log_debug(' '.join(cmd))
        try:
            return subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        except Exception as e:
            if ignore_failure:
                return ''
            raise e

    def _wait_for_expected_exit_code(self, task_name, expected_code, timeout=120, interval=2):
        args = ["filter=title = '%s'" % task_name, "warn=exit_code = %d" % expected_code]
        deadline = time.time() + timeout
        while time.time() < deadline:
            (ret, msg, perf) = self.core.simple_query('check_tasksched', args)
            if ret == status.WARNING:
                return True
            time.sleep(interval)
        log_error("Timed out waiting for task %s to report exit_code=%d" % (task_name, expected_code))
        return False

    def get_count(self, perf):
        if not perf:
            return -1
        (title, data) = perf.split('=')
        if not data:
            return -1
        (count, warn, crit) = data.split(';')
        return int(count)

    def check_ts_query(self, task, code):
        result = TestResult('Checking task %s'%task)
        task_name = self.tasks.get(task, 'NSCPSample_%s' % task)
        for i in [0, 1, 2, 3, 4]:
            # check_tasksched "filter=title = 'NSCPSample_CRIT'" "warn=exit_code != 3"
            args = ["filter=title = '%s'" % task_name, "warn=exit_code = %d"%i]
            log_debug(', '.join(args))
            (ret, msg, perf) = self.core.simple_query('check_tasksched', args)
            
            if i == code:
                result.assert_equals(ret, status.WARNING, 'Verify WARN result: %s'%msg)
            else:
                result.assert_equals(ret, status.OK, 'Verify OK result: %s'%msg)
            
        return result
        
    def run_test(self, cases = None):
        result = TestResult('Testing W32 task scheduler')
        for (state, code) in [('OK', 0), ('WARN', 1), ('CRIT', 2), ('LONG', 0)]:
            result.add(self.check_ts_query(state, code))
        return result

    def setup(self, plugin_id, prefix):
        self.reg = Registry.get(plugin_id)
        suffix = '%d_%d' % (int(time.time()), os.getpid())
        folder = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        script = os.path.join(folder, 'check_test.bat')
        self.tasks = {}
        log("Adding scheduled tasks")
        for state in ['OK', 'WARN', 'CRIT', 'LONG']:
            task_name = 'NSCPSample_%s_%s' % (state, suffix)
            self.tasks[state] = task_name
            task_command = '"%s" %s' % (script, state)
            self._run_schtasks(['/Delete', '/TN', task_name, '/F'], ignore_failure=True)
            self._run_schtasks(['/Create', '/SC', 'DAILY', '/TN', task_name, '/TR', task_command, '/ST', '00:00', '/F'])
            self._run_schtasks(['/Run', '/TN', task_name])

        log('Waiting for tasks to complete and report expected exit codes')
        for state in ['OK', 'WARN', 'CRIT', 'LONG']:
            code = self.expected_exit_codes[state]
            task_name = self.tasks[state]
            if not self._wait_for_expected_exit_code(task_name, code):
                raise Exception('Task %s did not report expected exit code %d in time' % (task_name, code))

    def install(self):
        conf = self.conf
        conf.set_string('/modules', 'test_tsch', 'CheckTaskSched')
        conf.set_string('/modules', 'pytest', 'PythonScript')
        conf.set_string('/settings/pytest/scripts', 'test_w32_tsch', __file__)
        conf.save()

    def teardown(self):
        for task_name in self.tasks.values():
            self._run_schtasks(['/Delete', '/TN', task_name, '/F'], ignore_failure=True)
        self.tasks = {}

    def help(self):
        pass

    def init(self, plugin_id):
        self.plugin_id = plugin_id
        self.reg = Registry.get(plugin_id)
        self.core = Core.get(plugin_id)
        self.conf = Settings.get(plugin_id)


all_tests = [Win32SchedTaskTest()]

def __main__(args):
    install_testcases(all_tests)
    
def init(plugin_id, plugin_alias, script_alias):
    init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
    log("shutdown")
    shutdown_testcases()
