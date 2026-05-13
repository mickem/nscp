"""Integration tests for the CheckHelpers module.

Covers every command CheckHelpers exposes plus the alias subsystem that
was added so admins can run aliases without enabling CheckExternalScripts
(see docs/setup/securing.md). Each test runs the command via
core.simple_query and asserts the returned status/message.

The alias section uses the new [/settings/check helpers/alias] location,
not the legacy [/settings/external scripts/alias]. We deliberately do
NOT load CheckExternalScripts in this test - this is the configuration
admins who want aliases-without-scripts will be running, and proving it
works end-to-end is the whole point.
"""

from NSCP import Settings, Registry, Core, log, status, log_error
from test_helper import BasicTest, TestResult, install_testcases, init_testcases, shutdown_testcases


class CheckHelpersTest(BasicTest):
    key = ''
    reg = None
    conf = None
    core = None

    def desc(self):
        return 'Integration tests for the CheckHelpers module'

    def title(self):
        return 'Test CheckHelpers'

    def setup(self, plugin_id, prefix):
        self.key = '_%stest_command' % prefix

    def teardown(self):
        None

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def query(self, command, args=None):
        if args is None:
            args = []
        return self.core.simple_query(command, args)

    def expect(self, result, command, args, expected_status, expected_msg_contains=None, label=None):
        label = label or command
        (ret, msg, perf) = self.query(command, args)
        result.assert_equals(ret, expected_status, '%s: status' % label)
        if expected_msg_contains is not None:
            result.assert_contains(expected_msg_contains, msg, '%s: message contains "%s"' % (label, expected_msg_contains))
        return (ret, msg, perf)

    # ------------------------------------------------------------------
    # Status-setting commands: check_ok / check_warning / check_critical
    # All three accept --message=<text> and return that status with that
    # message. They're the most basic primitives in the module.
    # ------------------------------------------------------------------

    def test_simple_status(self, ret):
        suite = TestResult('Simple status commands')

        self.expect(suite, 'check_ok', [], status.OK, label='check_ok (default)')
        self.expect(suite, 'check_ok', ['message=hello'], status.OK, 'hello', label='check_ok (message)')

        self.expect(suite, 'check_warning', [], status.WARNING, label='check_warning (default)')
        self.expect(suite, 'check_warning', ['message=careful'], status.WARNING, 'careful', label='check_warning (message)')

        self.expect(suite, 'check_critical', [], status.CRITICAL, label='check_critical (default)')
        self.expect(suite, 'check_critical', ['message=on fire'], status.CRITICAL, 'on fire', label='check_critical (message)')

        ret.add(suite)

    # ------------------------------------------------------------------
    # check_version returns the build version; we don't pin the exact
    # string, just assert OK + non-empty.
    # ------------------------------------------------------------------

    def test_version(self, ret):
        suite = TestResult('check_version')
        (rc, msg, _) = self.query('check_version', [])
        suite.assert_equals(rc, status.OK, 'check_version: status OK')
        suite.add_message(len(msg) > 0, 'check_version: returns a non-empty version string')
        ret.add(suite)

    # ------------------------------------------------------------------
    # check_always_{ok,warning,critical} wrap another command and force
    # its return code. They're how operators say "I want to be told this
    # ran, but I don't want it to alert".
    # ------------------------------------------------------------------

    def test_always_status(self, ret):
        suite = TestResult('check_always_* coerce wrapped result')

        # check_always_* use POSITIONAL args: first token is the wrapped
        # command, the rest are forwarded as that command's arguments.
        # Not `command=` / `arguments=` — those are reserved for check_negate
        # and check_multi which use named options.

        # Underlying check is CRITICAL; check_always_ok should turn it OK.
        self.expect(
            suite, 'check_always_ok',
            ['check_critical', 'message=ignored'],
            status.OK,
            label='check_always_ok wrapping check_critical -> OK')

        # Underlying check is OK; check_always_warning should turn it WARNING.
        self.expect(
            suite, 'check_always_warning',
            ['check_ok', 'message=ignored'],
            status.WARNING,
            label='check_always_warning wrapping check_ok -> WARNING')

        # Underlying check is OK; check_always_critical should turn it CRITICAL.
        self.expect(
            suite, 'check_always_critical',
            ['check_ok', 'message=ignored'],
            status.CRITICAL,
            label='check_always_critical wrapping check_ok -> CRITICAL')

        ret.add(suite)

    # ------------------------------------------------------------------
    # check_multi runs several commands and returns the worst status.
    # ------------------------------------------------------------------

    def test_multi(self, ret):
        suite = TestResult('check_multi takes worst result')

        # OK + WARNING -> WARNING
        self.expect(
            suite, 'check_multi',
            ['command=check_ok', 'command=check_warning'],
            status.WARNING,
            label='check_multi(OK, WARN) -> WARN')

        # OK + WARNING + CRITICAL -> CRITICAL
        self.expect(
            suite, 'check_multi',
            ['command=check_ok', 'command=check_warning', 'command=check_critical'],
            status.CRITICAL,
            label='check_multi(OK, WARN, CRIT) -> CRIT')

        # OK only -> OK
        self.expect(
            suite, 'check_multi',
            ['command=check_ok'],
            status.OK,
            label='check_multi(OK) -> OK')

        ret.add(suite)

    # ------------------------------------------------------------------
    # check_negate flips status codes per a mapping. With no mapping it's
    # the identity. We don't probe every combination here - just enough
    # to prove the command parses and dispatches.
    # ------------------------------------------------------------------

    def test_negate(self, ret):
        suite = TestResult('check_negate')

        # Default mapping: OK -> CRITICAL via --ok=critical.
        self.expect(
            suite, 'check_negate',
            ['ok=critical', 'command=check_ok'],
            status.CRITICAL,
            label='check_negate(ok->critical) on check_ok')

        # CRITICAL -> OK via --critical=ok.
        self.expect(
            suite, 'check_negate',
            ['critical=ok', 'command=check_critical'],
            status.OK,
            label='check_negate(critical->ok) on check_critical')

        ret.add(suite)

    # ------------------------------------------------------------------
    # check_timeout runs a wrapped command with an upper bound. We use a
    # generous timeout against a fast internal command so the test is
    # deterministic across CI agents.
    # ------------------------------------------------------------------

    def test_timeout(self, ret):
        suite = TestResult('check_timeout')

        self.expect(
            suite, 'check_timeout',
            ['timeout=10', 'command=check_ok', 'arguments=message=fast'],
            status.OK,
            'fast',
            label='check_timeout(10s, check_ok)')

        ret.add(suite)

    # ------------------------------------------------------------------
    # Aliases - the headline feature of this test file.
    #
    # Asserts that
    #  1. an alias defined under [/settings/check helpers/alias] dispatches
    #     to the right internal command, AND
    #  2. $ARGn$ / %ARGn% substitution into the alias's pre-declared
    #     argument list works as documented in docs/concepts/index.md.
    #
    # The security property we're indirectly proving: the alias's COMMAND
    # NAME is fixed by the local config and not the caller. We don't add
    # a negative test for that here because it's structurally guaranteed
    # by query_fallback's lookup path, not a runtime check.
    # ------------------------------------------------------------------

    def test_aliases(self, ret):
        suite = TestResult('CheckHelpers aliases')

        # Simple alias: name -> check_ok with a fixed message. No $ARG.
        self.expect(
            suite, 'tch_alias_ok',
            [],
            status.OK,
            'fixed via alias',
            label='alias dispatches to check_ok with fixed message')

        # Alias to a different status command. Proves the alias mechanism
        # isn't somehow OK-only.
        self.expect(
            suite, 'tch_alias_warn',
            [],
            status.WARNING,
            'warn via alias',
            label='alias dispatches to check_warning')

        # $ARG1$ substitution into a pre-declared argument template.
        self.expect(
            suite, 'tch_alias_msg',
            ['hello via $arg'],
            status.OK,
            'hello via $arg',
            label='$ARG1$ substituted into pre-declared message=')

        # %ARG1% variant - the alternative substitution syntax should work
        # exactly the same way.
        self.expect(
            suite, 'tch_alias_msg_pct',
            ['hello via pct'],
            status.OK,
            'hello via pct',
            label='%ARG1% substituted into pre-declared message=')

        # Missing $ARG (alias requires one, none supplied) - the command
        # still dispatches, the literal placeholder reaches check_ok which
        # echoes it back. The point of this assertion is to pin down the
        # CURRENT behaviour: aliases don't reject calls with too few args,
        # they just leave the placeholder in place.
        self.expect(
            suite, 'tch_alias_msg',
            [],
            status.OK,
            '$ARG1$',
            label='missing $ARG1$ leaves placeholder verbatim')

        ret.add(suite)

    # ------------------------------------------------------------------
    # Driver
    # ------------------------------------------------------------------

    def run_test(self, cases=None):
        ret = TestResult('CheckHelpers test suite')
        self.test_simple_status(ret)
        self.test_version(ret)
        self.test_always_status(ret)
        self.test_multi(ret)
        self.test_negate(ret)
        self.test_timeout(ret)
        self.test_aliases(ret)
        return ret

    # ------------------------------------------------------------------
    # Configuration. Enables CheckHelpers ONLY (no CheckExternalScripts)
    # and seeds the alias section with the entries the test expects.
    # ------------------------------------------------------------------

    def install(self):
        self.conf.set_string('/modules', 'pytest', 'PythonScript')

        # Load CheckHelpers under a named instance so the test's alias
        # definitions land under /settings/test_check_helpers/alias instead
        # of polluting a developer's own /settings/check helpers/alias.
        # Same pattern as test_external_script.py.
        self.conf.set_string('/modules', 'test_check_helpers', 'CheckHelpers')

        self.conf.set_string('/settings/pytest/scripts', 'test_check_helpers', 'test_check_helpers.py')

        # Alias definitions. Prefixed with `tch_` (Test Check Helpers) so
        # they don't collide with any aliases an operator might have on
        # their dev box.
        self.conf.set_string('/settings/test_check_helpers/alias', 'tch_alias_ok', 'check_ok message="fixed via alias"')
        self.conf.set_string('/settings/test_check_helpers/alias', 'tch_alias_warn', 'check_warning message="warn via alias"')
        self.conf.set_string('/settings/test_check_helpers/alias', 'tch_alias_msg', 'check_ok "message=$ARG1$"')
        self.conf.set_string('/settings/test_check_helpers/alias', 'tch_alias_msg_pct', 'check_ok "message=%ARG1%"')

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

    def require_boot(self):
        return True


instance = CheckHelpersTest()
all_tests = [instance]


def __main__(args):
    install_testcases(all_tests)


def init(plugin_id, plugin_alias, script_alias):
    init_testcases(plugin_id, plugin_alias, script_alias, all_tests)


def shutdown():
    shutdown_testcases()
