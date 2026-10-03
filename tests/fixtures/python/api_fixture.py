# Fixture for tests/pythonscript-api.test.ts.
#
# Every PythonScript API the scripting docs promise, each behind a query the
# test drives over REST (or, for the command-line handlers, through the CLI).
# A query answers with what the API returned, so the assertions live in the
# test and this file only has to call things and report.
#
# Lifecycle calls are appended to ${data-path}/pyapi-lifecycle.log, one line
# each, because the interesting ones (shutdown, a second init after a reload,
# __main__ under `nscp py execute`) happen where no query can see them.
import os

from NSCP import Core, Registry, Settings, log, log_debug, log_error, status

SETTINGS_PATH = '/settings/pyapi'

STATUS = {
    'ok': status.OK,
    'warning': status.WARNING,
    'critical': status.CRITICAL,
    'unknown': status.UNKNOWN,
}

plugin_id = None
my_alias = ''
seen_submissions = []
seen_events = []
seen_event_pb = []
seen_metrics = {}
# Deliveries per record (keyed by the record's `core`), and event_pb calls, so
# the test can tell a message delivered once from one delivered per record.
event_records = {}
event_pb_calls = [0]
# Channel handlers registered under the same name as the event: they must
# never be handed an event.
channel_collisions = []


def lifecycle(line):
    """Append one line to the lifecycle trace the test reads."""
    path = Core.get(plugin_id if plugin_id is not None else 0).expand_path('${data-path}')
    with open(os.path.join(path, 'pyapi-lifecycle.log'), 'a') as f:
        f.write(line + '\n')


def kv(args):
    """REST passes each option as one `key=value` token; a bare key is a flag."""
    out = {}
    for a in args:
        k, _, v = a.partition('=')
        out[k] = v
    return out


# --- Registry.simple_function ---------------------------------------------------

def py_echo(args):
    return (status.OK, 'args: ' + '|'.join(args), "'count'=%d;5;8" % len(args))


def py_status(args):
    o = kv(args)
    code = STATUS[o.get('status', 'ok')]
    return (code, 'status: ' + o.get('status', 'ok'), "'value'=%s;5;8" % o.get('value', '1'))


# --- error paths ------------------------------------------------------------------

def py_raise(args):
    raise ValueError('boom from py_raise')


def py_none(args):
    return None


def py_bad_shape(args):
    return 'not a tuple'


def py_short(args):
    return (status.WARNING,)


# --- Core ---------------------------------------------------------------------------

def py_nested(args):
    """Core.simple_query: run another query, here or in another module."""
    o = kv(args)
    rest = [a for a in args if not a.startswith('target=')]
    (code, msg, perf) = Core.get(plugin_id).simple_query(o['target'], rest)
    return (code, 'nested %s: %s' % (o['target'], msg), perf)


def py_exec(args):
    """Core.simple_exec: run a command-line handler of a module."""
    o = kv(args)
    (code, lines) = Core.get(plugin_id).simple_exec(o['module'], o['command'], [o.get('arg', '')])
    return (status.OK, 'exec %s: code=%s lines=%s' % (o['command'], code, '|'.join(lines)))


def py_submit(args):
    """Core.simple_submit on a channel; the test reads what arrived."""
    o = kv(args)
    (ok, resp) = Core.get(plugin_id).simple_submit(o['channel'], o.get('command', 'py_submitted'),
                                                   STATUS[o.get('status', 'warning')], o.get('message', 'from python'),
                                                   o.get('perf', ''))
    return (status.OK if ok else status.CRITICAL, 'submitted: %s %s' % (ok, resp))


def py_module(args):
    """Core.load_module / unload_module / reload, answering with what they returned."""
    o = kv(args)
    c = Core.get(plugin_id)
    action = o['action']
    if action == 'load':
        ok = c.load_module(o['name'], o.get('alias', ''))
    elif action == 'unload':
        ok = c.unload_module(o['name'])
    else:
        ok = c.reload(o['name'])
    return (status.OK, '%s %s: %s' % (action, o['name'], ok))


def py_expand(args):
    return (status.OK, Core.get(plugin_id).expand_path(args[0]))


# --- subscriptions, events, metrics ----------------------------------------------------

def on_submission(channel, source, command, code, message, perf):
    seen_submissions.append('%s|%s|%s|%d|%s|%s' % (channel, source, command, int(code), message, perf))
    return True


def on_reject(channel, source, command, code, message, perf):
    return False


def py_seen(args):
    """What the subscription and event handlers have recorded so far."""
    what = args[0]
    if what == 'submissions':
        return (status.OK, '\n'.join(seen_submissions) or 'none')
    if what == 'events':
        return (status.OK, '\n'.join(seen_events) or 'none')
    if what == 'event_pb':
        return (status.OK, '\n'.join(seen_event_pb) or 'none')
    if what == 'collisions':
        return (status.OK, '\n'.join(channel_collisions) or 'none')
    if what == 'event_counts':
        # Read in one go, under the GIL, so the two numbers belong together.
        per_record = sorted(event_records.values())
        return (status.OK, 'pb=%d records=%d min=%d max=%d' % (
            event_pb_calls[0], len(per_record), per_record[0] if per_record else 0,
            per_record[-1] if per_record else 0))
    if what == 'metrics':
        return (status.OK, '\n'.join('%s=%s' % (k, v) for k, v in sorted(seen_metrics.items())
                                     if 'pyapi' in k) or 'none')
    return (status.UNKNOWN, 'unknown: ' + what)


def on_event(event, data):
    seen_events.append('%s keys=%d' % (event, len(data)))
    record = data.get('core', '')
    event_records[record] = event_records.get(record, 0) + 1


def on_event_pb(event, request):
    seen_event_pb.append('%s %s %d' % (event, type(request).__name__, len(request)))
    event_pb_calls[0] += 1


def on_channel_named_like_event(channel, source, command, code, message, perf):
    channel_collisions.append('simple:%s' % channel)
    return True


def on_raw_channel_named_like_event(channel, message):
    channel_collisions.append('raw:%s' % channel)
    return (True, b'')


def fetch_metrics():
    return {'pyapi.fetched': 7}


def submit_metrics(metrics, request):
    seen_metrics.update(metrics)


# --- Settings ------------------------------------------------------------------------

def py_settings(args):
    s = Settings.get(plugin_id)
    o = kv(args)
    if 'set' in o:
        s.set_string(SETTINGS_PATH, 'scratch', o['set'])
    keys = ','.join(sorted(s.get_section(SETTINGS_PATH)))
    return (status.OK, 'greeting=%s scratch=%s int=%d bool=%s keys=%s' % (
        s.get_string(SETTINGS_PATH, 'greeting', 'unset'),
        s.get_string(SETTINGS_PATH, 'scratch', 'unset'),
        s.get_int(SETTINGS_PATH, 'number', -1),
        s.get_bool(SETTINGS_PATH, 'flag', False),
        keys))


def py_settings_bad(args):
    """Typed reads of a value that is not of that type."""
    s = Settings.get(plugin_id)
    return (status.OK, 'int=%d bool=%s missing=%s' % (
        s.get_int(SETTINGS_PATH, 'word', -1),
        s.get_bool(SETTINGS_PATH, 'word', True),
        s.get_string('/settings/pyapi/no such section', 'key', 'default')))


# --- command line --------------------------------------------------------------------

def py_cli_echo(args):
    return (status.OK, 'cli: ' + ' '.join(args))


def py_cli_fail(args):
    raise RuntimeError('boom from py_cli_fail')


# --- lifecycle -----------------------------------------------------------------------

def __main__(args):
    lifecycle('main %s' % ' '.join(args))


def init(pid, plugin_alias, script_alias):
    global plugin_id, my_alias
    plugin_id = pid
    my_alias = script_alias
    lifecycle('init %s %s' % (plugin_alias, script_alias))

    s = Settings.get(plugin_id)
    s.register_path(SETTINGS_PATH, 'Python API fixture', 'Settings the pythonscript-api fixture registers')
    s.register_key(SETTINGS_PATH, 'greeting', 'string', 'Greeting', 'What py_settings reports', 'hello')

    reg = Registry.get(plugin_id)
    reg.simple_function('py_echo', py_echo, 'Echo the arguments back')
    reg.simple_function('py_status', py_status, 'Return the status named by status=')
    reg.simple_function('py_raise', py_raise, 'Raise from a handler')
    reg.simple_function('py_none', py_none, 'Return None from a handler')
    reg.simple_function('py_bad_shape', py_bad_shape, 'Return a string instead of a tuple')
    reg.simple_function('py_short', py_short, 'Return a one-element tuple')
    reg.simple_function('py_nested', py_nested, 'Core.simple_query')
    reg.simple_function('py_exec', py_exec, 'Core.simple_exec')
    reg.simple_function('py_submit', py_submit, 'Core.simple_submit')
    reg.simple_function('py_module', py_module, 'Core.load_module / unload_module / reload')
    reg.simple_function('py_expand', py_expand, 'Core.expand_path')
    reg.simple_function('py_seen', py_seen, 'What the handlers recorded')
    reg.simple_function('py_settings', py_settings, 'Settings get/set/get_section')
    reg.simple_function('py_settings_bad', py_settings_bad, 'Typed reads of the wrong type')

    reg.simple_cmdline('py_cli_echo', py_cli_echo)
    reg.simple_cmdline('py_cli_fail', py_cli_fail)

    reg.simple_subscription('PYCHAN', on_submission)
    reg.simple_subscription('PYREJECT', on_reject)

    reg.event('system.cpu:py_rt', on_event)
    reg.event_pb('system.cpu:py_rt', on_event_pb)
    # Registered after the event handlers, under the event's name: a channel
    # and an event are different things that happen to share it.
    reg.simple_subscription('system.cpu:py_rt', on_channel_named_like_event)
    reg.subscription('system.cpu:py_rt', on_raw_channel_named_like_event)

    reg.fetch_metrics(fetch_metrics)
    reg.submit_metrics(submit_metrics)
    log('pyapi fixture loaded as %s' % script_alias)
    log_debug('pyapi fixture debug line')


def shutdown():
    lifecycle('shutdown %s' % my_alias)
