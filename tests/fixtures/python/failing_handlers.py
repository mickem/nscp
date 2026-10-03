# Fixture for tests/pythonscript-api.test.ts: every kind of handler, each one
# failing. The agent must log each failure, keep serving, and keep calling the
# other scripts' handlers of the same kind.
from NSCP import Registry, status


def on_event(event, data):
    raise RuntimeError('boom from on_event')


def fetch_metrics():
    raise RuntimeError('boom from fetch_metrics')


def submit_metrics(metrics, request):
    raise RuntimeError('boom from submit_metrics')


def on_submission(channel, source, command, code, message, perf):
    raise RuntimeError('boom from on_submission')


def cli_none(args):
    return None


def cli_bad_shape(args):
    return 'not a tuple'


def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.event('system.cpu:py_rt_raise', on_event)
    reg.fetch_metrics(fetch_metrics)
    reg.submit_metrics(submit_metrics)
    reg.simple_subscription('PYRAISE', on_submission)
    reg.simple_cmdline('py_cli_none', cli_none)
    reg.simple_cmdline('py_cli_bad_shape', cli_bad_shape)
