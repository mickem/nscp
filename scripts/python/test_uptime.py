"""
Integration tests for the configurable timezone introduced for `check_uptime`
(issues #365, #452, #590).

The timezone is owned per-module: each plugin caches the value of
`/settings/default/timezone` in its `loadModuleEx` (no settings-core
persistence). To switch timezone at runtime in a test we therefore have to
reload the affected module so its `loadModuleEx` runs again and re-reads the
setting.

For both `local` and `utc` we exercise `check_uptime` (CheckSystem on
Windows / CheckSystemUnix on Linux):

* the default detail-syntax surfaces the configured zone via `${tz}`;
* the `--max-unit` argument controls the granularity of `${uptime}`
  (e.g. `max-unit=d` caps the result at days);
* duration thresholds with unit suffixes (`30m`, `2h`, `1d`, `2w`)
  parse the same on both platforms (regression for #452).
"""

from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import (
    BasicTest,
    TestResult,
    install_testcases,
    init_testcases,
    shutdown_testcases,
)
import os
from time import time


# Alias used to register CheckSystem so we can reload it without affecting
# any other CheckSystem instance loaded by other tests.
SYSTEM_ALIAS = "test_uptime_system"

# CheckSystem maps to a different DLL/SO name on Windows vs. Linux.
SYSTEM_MODULE = "CheckSystem" if os.name == "nt" else "CheckSystemUnix"


class UptimeTimezoneTest(BasicTest):
    """Validate `check_uptime` under both `local` and `utc`."""

    reg = None
    conf = None
    core = None

    def desc(self):
        return "Testcase for check_uptime configurable timezone and max-unit (#365/#452/#590)"

    def title(self):
        return "check_uptime timezone tests"

    def setup(self, plugin_id, prefix):
        pass

    def teardown(self):
        pass

    # ------------------------------------------------------------------ helpers

    def _set_timezone(self, tz):
        """Write the configured timezone and reload CheckSystem so the
        per-module cache is refreshed."""
        self.conf.set_string("/settings/default", "timezone", tz)
        # The module reads the timezone in loadModuleEx; reload picks it up.
        self.core.reload(SYSTEM_ALIAS)
        # Give the module a moment to settle.
        sleep(500)

    def _query(self, args):
        return self.core.simple_query("check_uptime", args)

    # ------------------------------------------------------------ uptime tests

    def _run_uptime_for(self, tz, expected_label):
        result = TestResult("check_uptime with timezone=%s" % tz)
        self._set_timezone(tz)

        # 1. Default detail-syntax surfaces the configured zone via `(${tz})`.
        #    Override warn/crit with non-triggering thresholds so the test does
        #    not depend on how long the host has been up (the built-in defaults
        #    `warn=uptime<2d` / `crit=uptime<1d` would alert on freshly booted
        #    test machines).
        (rc, msg, _perf) = self._query(
            ["show-all", "warn=uptime < 0", "crit=uptime < 0"]
        )
        result.add_message(
            rc == status.OK,
            "default check_uptime should be OK with timezone=%s: rc=%s msg=%s"
            % (tz, rc, msg),
            "Expected OK",
        )
        result.assert_contains(
            expected_label,
            msg,
            "default detail-syntax should contain the configured tz label '%s' (got %r)"
            % (expected_label, msg),
        )
        result.assert_contains("boot:", msg, "default output should mention boot:")
        result.assert_contains("uptime:", msg, "default output should mention uptime:")

        # The opposite label must not leak through under the configured zone.
        # (Skip this assertion for the value where local==UTC could be true,
        # but treating the labels as opaque strings is fine because the
        # implementation always writes "local" or "UTC", never both.)
        opposite = "UTC" if expected_label == "local" else "local"
        result.assert_not_contains(
            opposite,
            msg,
            "configured zone is %s; opposite label '%s' should not appear (got %r)"
            % (tz, opposite, msg),
        )

        # 2. The --max-unit argument controls granularity of ${uptime} (#590).
        #    With max-unit=d, the rendered uptime must not contain a week
        #    suffix even on hosts that have been up longer than a week.
        #    Override warn/crit again to keep the result OK regardless of
        #    actual host uptime.
        (rc, msg, _perf) = self._query(
            [
                "show-all",
                "max-unit=d",
                "warn=uptime < 0",
                "crit=uptime < 0",
                "detail-syntax=uptime=${uptime} tz=${tz}",
            ]
        )
        result.add_message(
            rc == status.OK,
            "max-unit=d should resolve under timezone=%s: rc=%s msg=%s"
            % (tz, rc, msg),
            "Expected OK",
        )
        result.assert_contains(
            "uptime=",
            msg,
            "${uptime} placeholder should be substituted (got %r)" % msg,
        )
        # Reject the week suffix: max-unit=d must roll weeks into days.
        result.assert_not_contains(
            "w ",
            msg,
            "max-unit=d should not produce a week suffix (got %r)" % msg,
        )
        result.assert_contains(
            "tz=%s" % expected_label,
            msg,
            "${tz} placeholder should resolve to %r (got %r)" % (expected_label, msg),
        )

        # 2b. An invalid max-unit value should be rejected with UNKNOWN, not
        #     silently accepted.
        (rc, msg, _perf) = self._query(["show-all", "max-unit=foo"])
        result.assert_equals(
            rc,
            status.UNKNOWN,
            "max-unit=foo should fail with UNKNOWN (got rc=%s, msg=%s)" % (rc, msg),
        )

        # 3. Duration thresholds with unit suffixes must parse on both
        #    platforms (#452). The expression `uptime < 30m` previously
        #    misbehaved on Windows because the original suffix was lost.
        for spec in ["30m", "2h", "1d", "2w"]:
            (rc, msg, _perf) = self._query(
                ["show-all", "warn=uptime < %s" % spec, "detail-syntax=ok"]
            )
            result.add_message(
                rc != status.UNKNOWN,
                "uptime < %s should parse cleanly under timezone=%s (rc=%s, msg=%s)"
                % (spec, tz, rc, msg),
                "Got UNKNOWN",
            )

        return result

    # ----------------------------------------------------------------- runner

    def run_test(self, cases=None):
        master = TestResult("Testing check_uptime under local and utc")

        # Make sure the module under test is loaded before we start toggling
        # settings and reloading it.
        self.core.load_module(SYSTEM_MODULE, SYSTEM_ALIAS)

        for tz, label in [("local", "local"), ("utc", "UTC")]:
            master.add(self._run_uptime_for(tz, label))

        # Restore the default so we don't poison any subsequent test in the
        # same run.
        self._set_timezone("local")
        return master

    # ----------------------------------------------------------------- harness

    def install(self):
        self.conf.set_string("/modules", SYSTEM_ALIAS, SYSTEM_MODULE)
        self.conf.set_string("/modules", "pytest", "PythonScript")
        self.conf.set_string(
            "/settings/pytest/scripts", "test_uptime", "test_uptime.py"
        )

        # Default timezone for the run; individual test cases override.
        self.conf.set_string("/settings/default", "timezone", "local")

        self.conf.save()

    def uninstall(self):
        pass

    def help(self):
        pass

    def init(self, plugin_id):
        self.reg = Registry.get(plugin_id)
        self.conf = Settings.get(plugin_id)
        self.core = Core.get(plugin_id)

    def shutdown(self):
        pass

    def require_boot(self):
        return True


instance = UptimeTimezoneTest()
all_tests = [instance]


def __main__(args):
    install_testcases(all_tests)


def init(plugin_id, plugin_alias, script_alias):
    init_testcases(plugin_id, plugin_alias, script_alias, all_tests)


def shutdown():
    shutdown_testcases()
