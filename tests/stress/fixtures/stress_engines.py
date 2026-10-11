# Python half of the stress harness's engine fixture (tests/stress/lib/engines.ts).
#
# Every handler answers with a message naming the engine and echoing the token
# it was given, so a load generator can tell a correct answer from one that was
# crossed with a concurrent request's. The slow handlers park in time.sleep(),
# which is what real check code does and what must release the interpreter
# lock: while one sleeps, every other Python check has to keep answering. They
# mark themselves running with a file in the directory /settings/stress
# `markers` names, so a scenario can see that several are inside the agent at
# once rather than infer it from a clock.
import os
import time

from NSCP import Core, Registry, Settings, status

plugin_id = None
marker_dir = ''


def token(args):
    return args[0] if args else '-'


class running(object):
    """A marker file present exactly while the handler runs."""

    def __init__(self, name):
        self.path = os.path.join(marker_dir, name + '.running') if marker_dir else None

    def __enter__(self):
        if self.path:
            with open(self.path, 'w') as f:
                f.write('running\n')
        return self

    def __exit__(self, *exc):
        if self.path:
            try:
                os.remove(self.path)
            except OSError:
                pass
        return False


def py_ok(args):
    return (status.OK, 'py ' + token(args), '')


def py_sleep(args):
    """py_sleep <ms> [marker]: time.sleep, holding a marker while parked."""
    ms = int(args[0]) if args else 0
    with running(args[1] if len(args) > 1 else 'py_sleep'):
        time.sleep(ms / 1000.0)
    return (status.OK, 'py slept %d' % ms, '')


def py_via_lua(args):
    """Python -> core -> Lua: the interpreter lock must be released around the core call."""
    (code, msg, perf) = Core.get(plugin_id).simple_query('lua_ok', [token(args)])
    return (code, 'py>' + msg, perf)


def py_via_lua_sleep(args):
    """Python waits on a slow Lua check through the core: neither lock may be held across it."""
    ms = args[0] if args else '0'
    marker = args[1] if len(args) > 1 else 'py_via_lua_sleep'
    with running(marker):
        (code, msg, perf) = Core.get(plugin_id).simple_query('lua_sleep', [ms])
    return (code, 'py>' + msg, perf)


def init(pid, plugin_alias, script_alias):
    global plugin_id, marker_dir
    plugin_id = pid
    marker_dir = Settings.get(plugin_id).get_string('/settings/stress', 'markers', '')
    reg = Registry.get(plugin_id)
    reg.simple_function('py_ok', py_ok, 'Answer OK with the token')
    reg.simple_function('py_sleep', py_sleep, 'time.sleep for the given ms')
    reg.simple_function('py_via_lua', py_via_lua, 'Python -> core -> lua_ok')
    reg.simple_function('py_via_lua_sleep', py_via_lua_sleep, 'Python -> core -> lua_sleep')


def shutdown():
    pass
