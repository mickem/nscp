from typing import List

from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import os


class BasicTest:

    def __init__(self):
        pass

    def desc(self):
        return 'TODO: Describe: %s'%self.title()

    def title(self):
        pass

    def setup(self, plugin_id, prefix):
        pass

    def teardown(self):
        pass

    def run_test(self, cases = None):
        result = TestResult('run_test')
        result.add_message(False, 'TODO add implementation')
        return result

    def install(self):
        pass

    def uninstall(self):
        pass

    def help(self):
        pass

    def init(self, plugin_id):
        pass

    def shutdown(self):
        pass

    def require_boot(self):
        return False


class TestManager:

    suites : List[BasicTest]= []
    prefix = ''
    plugin_id = None
    plugin_alias = None
    script_alias = None
    show_all = False
    cases = []

    def __init__(self, plugin_id = 0, plugin_alias = '', script_alias = ''):
        if script_alias:
            self.prefix = '%s_'%script_alias
        self.plugin_id = plugin_id
        self.plugin_alias = plugin_alias
        self.script_alias = script_alias
        self.suites = []
        self.show_all = False
        self.cases = []

    def set_show_ok(self):
        self.show_all = True

    def add_case(self, cases : List[str]):
        self.cases.extend(cases)

    def add(self, suites : List[BasicTest]):
        for suite in suites:
            self.suites.append(suite)

    def run(self):
        result = TestResult('Test result for %d suites'%len(self.suites))
        for suite in self.suites:
            suite.setup(self.plugin_id, self.prefix)
            suite_result = TestResult('Running suite: %s'%suite.title())
            cases = self.cases if self.cases else None
            suite_result.append(suite.run_test(cases))
            result.append(suite_result)
            result.add_message(suite_result.is_ok(), 'Result from suite: %s'%suite.title())
            suite.teardown()
        return result

    def init(self):
        for suite in self.suites:
            suite.init(self.plugin_id)

    def destroy(self):
        self.suites = []
        self.prefix = ''
        self.plugin_id = None
        self.plugin_alias = None
        self.script_alias = None

    def install(self):
        boot = False
        for suite in self.suites:
            suite.install()
            if suite.require_boot():
                boot = True

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
            suite.uninstall()
        for suite in self.suites:
            suite.shutdown()

test_manager : TestManager|None = None


def install_testcases(tests: List[BasicTest]):
    manager = create_test_manager()
    manager.add(tests)
    manager.install()

def init_testcases(plugin_id, plugin_alias, script_alias, tests):
    manager = create_test_manager(plugin_id, plugin_alias, script_alias)
    manager.add(tests)
    manager.init()

def shutdown_testcases():
    if get_test_manager():
        get_test_manager().shutdown()
    destroy_test_manager()

def get_test_manager() -> TestManager:
    global test_manager
    return test_manager

def destroy_test_manager():
    global test_manager
    if test_manager:
        test_manager.destroy()
    test_manager = None

def create_test_manager(plugin_id = 0, plugin_alias = '', script_alias = '') -> TestManager:
    global test_manager
    if not test_manager:
        test_manager = TestManager(plugin_id, plugin_alias, script_alias)
        
        reg = Registry.get(plugin_id)
        
        reg.simple_cmdline('help', display_help)
        reg.simple_cmdline('install_python_test', install_tests)
        reg.simple_cmdline('run_python_test', run_tests)

        reg.simple_function('py_unittest', run_tests, 'Run python unit test suite')
        reg.simple_function('py_unittest_show_ok', set_show_ok, 'Set verbouse log')
        reg.simple_function('py_unittest_add_case', add_case, 'Set which cases to run')
    
    return test_manager
    
def install_tests():
    get_test_manager().install()
    return status.OK, 'installed?'

def run_tests(ignored=None):
    result = get_test_manager().run()
    return result.return_nagios(get_test_manager().show_all)

def set_show_ok(ignored=None):
    get_test_manager().set_show_ok()
    return status.OK, 'Done'

def add_case(cases):
    get_test_manager().add_case(cases)
    return status.OK, 'Done'

def display_help():
    return status.OK, 'TODO'


        
class TestResultEntry:
    status : bool = False
    desc : str= 'Unassigned result'
    error : str|None= None
    def __init__(self, status = False, desc = 'Unassigned result', error : str|None= None):
        self.status = status
        self.desc = desc
        self.error = error

    def log(self, show_all = False, prefix = '', indent = 0):
        if self.status:
            if show_all:
                log('%s%s%s'%(prefix, ''.rjust(indent, ' '), self))
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

    children = []
    def __init__(self, title, status = True, desc = 'Unassigned result', error = None, children=None):
        super().__init__(status, desc, error)
        self.title = title
        self.children = []
        if children:
            self.extend(children)

    def log(self, show_all = False, prefix = '', indent = 0):
        start = '%s%s'%(prefix, ''.rjust(indent, ' '))
        if self.status:
            if show_all:
                log('%s%s'%(start, self))
            log_debug('%s%s'%(start, self))
        else:
            log_error('%s%s'%(start, self))
        for c in self.children:
            c.log(show_all, prefix, indent+1)
    
    def is_ok(self):
        return self.status

    def count(self):
        total_count = 0
        ok_count = 0
        #if self.status:
        #    ok_count = 1

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

    def add_message(self, status : bool, message : str, error : str|None= None):
        e = TestResultEntry(status, message, error)
        e.log()
        self.append(e)
        
    def assert_equals(self, s1, s2, msg):
        self.add_message(s1 == s2, msg, '"%s" != "%s"'%(s1, s2))
        
    def assert_gt(self, v1, v2, msg):
        self.add_message(v1 > v2, msg, '%d should be greater then %d'%(v1, v2))
    def assert_lt(self, v1, v2, msg):
        self.add_message(v1 < v2, msg, '%d should be less then %d'%(v1, v2))

    def assert_contains(self, s1, s2, msg):
        if s1 == s2:
            self.add_message(s1 in s2 or s2 in s1, msg, '"%s" (contains) "%s"'%(s1, s2))
        elif s1 is None or s2 is None:
            self.add_message(False, msg, '"%s" (contains) "%s"'%(s1, s2))
        else:
            self.add_message(s1 in s2 or s2 in s1, msg, '"%s" (contains) "%s"'%(s1, s2))

    def assert_not_contains(self, s1, s2, msg):
        if s1 == s2:
            self.add_message(False, msg, '"%s" (equals) "%s"'%(s1, s2))
        elif s1 is None or s2 is None:
            self.add_message(True, msg, '"%s" (is null?) "%s"'%(s1, s2))
        else:
            self.add_message(not (s1 in s2 or s2 in s1), msg, '"%s" (does not contains) "%s"'%(s1, s2))

    def add_entry(self, e):
        self.append(e)

    def add(self, result):
        self.extend(result)

    def return_nagios(self, show_all = False):
        (total, ok) = self.count()
        self.log(show_all, ' | ')
        if total == ok:
            return status.OK, "OK: %d test(s) successfull"%(total)
        else:
            return status.CRITICAL, "ERROR: %d/%d test(s) failed"%(total-ok, total)

