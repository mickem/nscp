# Python Scripts

In addition to executing Python scripts as regular external scripts, NSClient++ can also execute them
in-process via the **PythonScript** module. The benefit is that scripts can call NSClient++'s APIs
directly (run checks, listen on channels, register handlers, read/write settings) and keep state
between invocations.

To add a Python script as an internal script you need to load the PythonScript module and register
the script:

```
nscp py add --script my_script.py
```

This produces:

```ini
[/modules]
PythonScript = enabled

[/settings/python/scripts]
my_script = my_script.py
```

## Lifecycle functions

A script may define any of three lifecycle functions:

- `__main__` — invoked when the script is run from the command line
- `init` — invoked when the script is loaded by the module
- `shutdown` — invoked when the module unloads or NSClient++ stops

All three are optional. If a function is missing it is simply skipped.

### `__main__`

`__main__` is invoked when the script is run from the command line. Use it for installation,
configuration helpers, or anything else an administrator might want to run interactively:

```
nscp py execute --script my_script.py install --root /tmp
```

The function receives the command-line arguments as a list of strings:

```python
from NSCP import log

def __main__(args):
    log("My arguments are: %s" % args)
```

Output:

```
nscp py execute --script my_script.py install --root /tmp
L     python My arguments are: ['install', '--root', '/tmp']
```

### `init`

`init` is invoked when the script is loaded and is where you obtain the values needed to talk back
to NSClient++. It receives three arguments:

- `plugin_id` — internal identifier of the plugin instance; required by `Registry.get()`,
  `Core.get()`, and `Settings.get()`
- `plugin_alias` — the alias the PythonScript module is loaded under
- `script_alias` — the key the script is registered under in `[/settings/<alias>/scripts]`

```python
from NSCP import log

def init(plugin_id, plugin_alias, script_alias):
    log("*** plugin_id: %d, plugin_alias: %s, script_alias: %s"
        % (plugin_id, plugin_alias, script_alias))
```

A typical startup log looks like:

```
D     python boot python
D     python Prepare python
D     python init python
D     python Adding script: osmc (c:\source\build\x64\dev\scripts\python\osmc.py)
D     python Loading python script: c:\source\build\x64\dev\scripts\python\osmc.py
D     python Lib path: c:\source\build\x64\dev\scripts\python\lib
L     python *** plugin_id: 0, plugin_alias: python, script_alias: my_script
```

#### `plugin_alias` versus `script_alias`

Consider:

```ini
[/modules]
ps1 = PythonScript
ps2 = PythonScript

[/settings/ps1/scripts]
ms1 = my_script.py
ms2 = my_script.py

[/settings/ps2/scripts]
ms1 = my_script.py
ms2 = my_script.py
```

This loads the same script four times with the following arguments:

| plugin_id | plugin_alias | script_alias |
|-----------|--------------|--------------|
| 1         | ps1          | ms1          |
| 1         | ps1          | ms2          |
| 2         | ps2          | ms1          |
| 2         | ps2          | ms2          |

If a script may legitimately be loaded multiple times, store its configuration under a path that
includes both aliases, e.g. `/settings/<script_name>/<plugin_alias>/<script_alias>/`.

<!-- @formatter:off -->
!!! warning
    Loading the same script multiple times is discouraged — the CLI does not provide shortcuts for it —
    but it is supported for complex scenarios.
<!-- @formatter:on -->

### `shutdown`

`shutdown` is invoked when NSClient++ stops or when the PythonScript module is unloaded. It takes
**no arguments**:

```python
from NSCP import log

def shutdown():
    log("Good bye")
```

## API

The module is imported as `NSCP` and exposes:

- Three wrapper classes: **`Settings`**, **`Registry`**, **`Core`** — each obtained via `.get(plugin_id)`
- An enum: **`status`** — Nagios-style return codes
- Free functions: **`log`**, **`log_error`**, **`log_debug`**, **`sleep`**

Most operations have a "simple" variant that uses plain Python types, and a "raw" variant that takes
serialized protobuf messages for full access to the underlying API. Use the simple form unless you
need fields the simple form doesn't expose.

### Free functions

#### `log` / `log_error` / `log_debug`

```python
log(message)
log_error(message)
log_debug(message)
```

Write a message to the NSClient++ log at the corresponding level (`info`, `error`, `debug`).
Messages appear on the command line and in any configured log file.

```python
from NSCP import log, log_error, log_debug

log("Informational message")
log_error("Something went wrong")
log_debug("Internal state: %s" % state)
```

#### `sleep`

```python
sleep(milliseconds)
```

Sleep for the given number of milliseconds. Use this instead of `time.sleep()`: Python is
single-threaded with respect to the GIL, so a blocking `time.sleep()` inside an NSClient++ script
prevents every other Python callback from running. `sleep()` releases the GIL while it waits.

```python
from NSCP import sleep, log_debug

log_debug("Waiting for 1 second")
sleep(1000) # Other taska can no execute while we wait
log_debug("It is now one second later")
```

### `status` enum

| Key      | Value | Description                |
|----------|-------|----------------------------|
| OK       | 0     | Nagios OK                  |
| WARNING  | 1     | Nagios warning             |
| CRITICAL | 2     | Nagios critical            |
| UNKNOWN  | 3     | Nagios unknown             |

### Registry

`Registry` lets a script publish itself into NSClient++ — registering check commands, command-line
commands, channel subscriptions, event handlers, and metrics providers.

#### `Registry.get`

```python
Registry.get(plugin_id)
```

Obtain an instance of `Registry` bound to the calling plugin. Pass the `plugin_id` you received in
`init`.

#### `Registry.function`

```python
Registry.function(query_name, query_function, description)
```

Register a **check query** with a raw, protobuf-based callback. Use `simple_function` instead unless
you need fields it doesn't expose.

| Option           | Description                                              |
|------------------|----------------------------------------------------------|
| `query_name`     | The check command name (e.g. `check_py_test`)            |
| `query_function` | Callback invoked when the query runs                     |
| `description`    | Help text shown by `<command> help` and in the WEB UI    |

The callback receives the command name and a serialized `QueryRequestMessage`, and must return a
2-tuple `(return_code, response_bytes)` where `response_bytes` is a serialized
`QueryResponseMessage`:

```python
def my_function(command, request):
    request_message = plugin_pb2.QueryRequestMessage()
    request_message.ParseFromString(request)
    # ... build response ...
    response_message = plugin_pb2.QueryResponseMessage()
    return (status.OK, response_message.SerializeToString())
```

**Example:**

```python
from NSCP import log, Registry, status
import plugin_pb2

def my_function(command, request):
    request_message = plugin_pb2.QueryRequestMessage()
    request_message.ParseFromString(request)
    log('Got command: %s' % request_message.payload[0].command)
    response_message = plugin_pb2.QueryResponseMessage()
    return (status.OK, response_message.SerializeToString())

def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.function('check_py_test', my_function, 'This is a sample python function')
```

#### `Registry.simple_function`

```python
Registry.simple_function(query_name, query_function, description)
```

Register a check query with a simple Python callback — no protobuf required.

| Option           | Description                                          |
|------------------|------------------------------------------------------|
| `query_name`     | The check command name                               |
| `query_function` | Callback invoked when the query runs                 |
| `description`    | Help text shown by `<command> help`                  |

The callback receives the arguments as a list of strings and must return a 3-tuple
`(status, message, perfdata)`:

```python
def my_function(args):
    return (status.OK, "This is the message", "'count'=123;200;600")
```

**Example:**

```python
from NSCP import log, Registry, status

def my_function(args):
    log('Got arguments: %s' % args)
    return (status.OK, "This is the message", "'count'=123;200;600")

def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.simple_function('check_py_test', my_function, 'This is a sample python function')
```

#### `Registry.cmdline`

```python
Registry.cmdline(command_name, function)
```

Register a **command-line command** (invoked via `nscp client --command ...` or
`nscp ext --command ...`) with a protobuf-based callback. Use `simple_cmdline` unless you need raw
access.

| Option         | Description                                                       |
|----------------|-------------------------------------------------------------------|
| `command_name` | The command name to expose                                        |
| `function`     | Callback invoked when the command runs                            |

Callback signature: `(command, request) -> (return_code, response_bytes)`, where `request` is a
serialized `ExecuteRequestMessage` and `response_bytes` is a serialized `ExecuteResponseMessage`.

```python
from NSCP import log, Registry, status
import plugin_pb2

def my_function(command, request):
    request_message = plugin_pb2.ExecuteRequestMessage()
    request_message.ParseFromString(request)
    log('Got command: %s' % request_message.payload[0].command)
    response_message = plugin_pb2.ExecuteResponseMessage()
    return (0, response_message.SerializeToString())

def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.cmdline('do_foo', my_function)
```

#### `Registry.simple_cmdline`

```python
Registry.simple_cmdline(command_name, function)
```

Register a command-line command with a simple callback. The callback receives the arguments as a
list of strings and returns a 2-tuple `(exit_code, message)`:

```python
def my_function(args):
    return (0, "This is the message")
```

**Example:**

```python
from NSCP import log, Registry

def my_function(args):
    log('Got arguments: %s' % args)
    return (1, "Everything is awesome")

def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.simple_cmdline('do_something', my_function)
```

#### `Registry.subscription`

```python
Registry.subscription(channel, function)
```

Subscribe to a **channel** (think passive check submissions) with a protobuf-based handler. Use
`simple_subscription` unless you need raw access.

| Option    | Description                          |
|-----------|--------------------------------------|
| `channel` | The channel name to subscribe to     |
| `function`| Callback invoked when a message arrives |

Callback signature: `(channel, message) -> (success_bool, response_bytes)`. `message` is a memoryview
over a serialized `SubmitRequestMessage`.

```python
def my_function(channel, message):
    request = plugin_pb2.SubmitRequestMessage()
    request.ParseFromString(bytes(message))
    # ... process ...
    return (True, b"")
```

#### `Registry.simple_subscription`

```python
Registry.simple_subscription(channel, function)
```

Subscribe to a channel with a simple per-payload callback.

| Option    | Description                          |
|-----------|--------------------------------------|
| `channel` | The channel name to subscribe to     |
| `function`| Callback invoked per submitted check |

Callback signature: `(channel, source, command, status, message, perf) -> bool`. Return `True` to
indicate success.

**Example — suppress repeated NSCA submissions:**

```python
from NSCP import Registry, Core, status, log

g_last_message = ''
g_plugin_id = 0

def filter_nsca(channel, source, command, code, message, perf):
    global g_last_message, g_plugin_id
    if message != g_last_message:
        g_last_message = message
        log("Sending: %s" % message)
        core = Core.get(g_plugin_id)
        core.simple_submit('NSCA', command, status.CRITICAL,
                           message.encode('utf-8'), perf)
        return True
    else:
        log("Suppressing duplicate message")
        return True

def init(plugin_id, plugin_alias, script_alias):
    global g_plugin_id
    g_plugin_id = plugin_id
    reg = Registry.get(plugin_id)
    reg.simple_subscription('filter_nsca', filter_nsca)
```

Then point a real-time filter at the synthetic `filter_nsca` channel instead of `NSCA`:

```ini
[/modules]
PythonScript=enabled
CheckEventLog=enabled
NSCAClient=enabled

[/settings/python/scripts]
filter_nsca=filter_nsca.py

[/settings/eventlog/real-time]
enabled = true

[/settings/eventlog/real-time/filters/login]
log=Security
filter=id=4624
target=filter_nsca
detail syntax=%(id)
```

#### `Registry.event` / `Registry.event_pb`

```python
Registry.event(event_name, event_function)
Registry.event_pb(event_name, event_function)
```

Register a handler for a named event. The two variants differ in how the event payload is delivered
to the callback:

- `event` — payload is delivered as a Python `dict`. Callback signature: `(event_name, data) -> None`.
- `event_pb` — payload is delivered as a raw bytes buffer (serialized protobuf). Callback signature:
  `(event_name, request_bytes)`. The return value is ignored.

**Example (`event`):**

```python
from NSCP import log, Registry

def on_event(event, data):
    log('Got event: %s with key=%s' % (event, data.get('key')))

def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.event('eventlog:login', on_event)
```

#### `Registry.submit_metrics`

```python
Registry.submit_metrics(callable)
```

Register a function that **receives** metrics each time NSClient++ publishes a metrics snapshot.
Useful for forwarding metrics to a custom backend.

Callback signature: `(metrics, "") -> None`, where `metrics` is a flat `dict` of dotted-name keys to
stringified values.

```python
from NSCP import Registry, log

def on_metrics(metrics, _):
    for key, value in metrics.items():
        log("metric %s = %s" % (key, value))

def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.submit_metrics(on_metrics)
```

#### `Registry.fetch_metrics`

```python
Registry.fetch_metrics(callable)
```

Register a function that **produces** metrics on demand. The callback takes no arguments and returns
a `dict` whose values may be strings, ints, or floats; numeric values are emitted as gauges.

```python
from NSCP import Registry

def my_metrics():
    return {
        "my_script.requests": 42,
        "my_script.latency_ms": 17.3,
        "my_script.status": "ok",
    }

def init(plugin_id, plugin_alias, script_alias):
    reg = Registry.get(plugin_id)
    reg.fetch_metrics(my_metrics)
```

#### `Registry.query`

```python
(return_code, response_bytes) = Registry.query(request_bytes)
```

Issue a raw query against the registry — used to enumerate modules, queries, channels, etc. The
`request_bytes` must be a serialized `RegistryRequestMessage`; the response is a serialized
`RegistryResponseMessage`.

### Core

The `Core` object exposes operations that interact with the running NSClient++ instance itself —
running queries, submitting passive results, loading and reloading modules, expanding paths.

#### `Core.get`

```python
Core.get(plugin_id)
```

Obtain a `Core` instance bound to the calling plugin.

#### `Core.simple_query`

```python
(code, message, perf) = Core.simple_query(query, arguments)
```

Run a check command by name with a list of argument strings.

| Option      | Description                          |
|-------------|--------------------------------------|
| `query`     | Name of the query to run             |
| `arguments` | List of `keyword=value` argument strings |
| `code`      | Nagios status code                   |
| `message`   | Resulting message                    |
| `perf`      | Resulting performance data           |

```python
from NSCP import Core, log

def init(plugin_id, plugin_alias, script_alias):
    core = Core.get(plugin_id)
    (code, message, perf) = core.simple_query("check_cpu", [])
    log(message)
```

#### `Core.query`

```python
(return_code, response_bytes) = Core.query(command, request_bytes)
```

Raw protobuf variant of `simple_query`. `request_bytes` is a serialized `QueryRequestMessage`;
`response_bytes` is a serialized `QueryResponseMessage`.

```python
from NSCP import Core, log
import plugin_pb2

def init(plugin_id, plugin_alias, script_alias):
    request_message = plugin_pb2.QueryRequestMessage()
    # ... populate request_message ...
    core = Core.get(plugin_id)
    (ret, response) = core.query("check_cpu", request_message.SerializeToString())
    response_message = plugin_pb2.QueryResponseMessage()
    response_message.ParseFromString(response)
```

#### `Core.simple_exec`

```python
(return_code, results) = Core.simple_exec(target, command, arguments)
```

Execute a command-line command (registered via `Registry.cmdline` / `Registry.simple_cmdline`)
against `target` (a remote NSClient++ instance or `"local"`/`""` for in-process). Returns the result
as a list of strings.

#### `Core.exec`

```python
(return_code, response_bytes) = Core.exec(target, request_bytes)
```

Raw protobuf variant of `simple_exec`.

#### `Core.simple_submit`

```python
(success, response) = Core.simple_submit(channel, command, code, message, perf)
```

Submit a passive check result on a channel.

| Option     | Description                                       |
|------------|---------------------------------------------------|
| `channel`  | Channel to submit to (e.g. `"NSCA"`, `"NRDP"`)    |
| `command`  | Check command name being reported                 |
| `code`     | `status.OK` / `status.WARNING` / `status.CRITICAL` / `status.UNKNOWN` |
| `message`  | Message text (bytes or str)                       |
| `perf`     | Performance data string                           |

```python
from NSCP import Core, status

def init(plugin_id, plugin_alias, script_alias):
    core = Core.get(plugin_id)
    core.simple_submit("NSCA", "check_py", status.OK,
                       "All good".encode("utf-8"), "")
```

#### `Core.submit`

```python
(success, error_message) = Core.submit(channel, request_bytes)
```

Raw protobuf variant of `simple_submit`. `request_bytes` is a serialized `SubmitRequestMessage`.

#### `Core.reload`

```python
Core.reload(module)
```

Reload the given module by name (e.g. `"CheckEventLog"`). Pass `"service"` to reload the entire
service.

#### `Core.load_module`

```python
Core.load_module(module, alias)
```

Load a module into the running NSClient++ instance. `alias` lets you load the same module multiple
times under different names; pass `""` for the default.

```python
from NSCP import Core

def init(plugin_id, plugin_alias, script_alias):
    core = Core.get(plugin_id)
    core.load_module("CheckEventLog", "")
```

#### `Core.unload_module`

```python
Core.unload_module(module_or_alias)
```

Unload a previously loaded module by name or alias.

```python
from NSCP import Core

def init(plugin_id, plugin_alias, script_alias):
    core = Core.get(plugin_id)
    core.unload_module("CheckEventLog")
```

#### `Core.expand_path`

```python
path = Core.expand_path(path_expression)
```

Expand a path containing `${...}` placeholders (e.g. `${scripts}`, `${modules}`, `${base-path}`).

```python
from NSCP import Core, log

def init(plugin_id, plugin_alias, script_alias):
    core = Core.get(plugin_id)
    path = core.expand_path("${scripts}/python/myscript.py")
    log('The script path is: %s' % path)
```

### Settings

`Settings` wraps the configuration store. Read and write are supported; written values are
**in-memory only** until `save()` is called.

#### `Settings.get`

```python
Settings.get(plugin_id)
```

Obtain a `Settings` instance bound to the calling plugin.

```python
from NSCP import Settings

def init(plugin_id, plugin_alias, script_alias):
    config = Settings.get(plugin_id)
    value = config.get_string("/modules", "PythonScript", "disabled")
```

#### `Settings.get_section`

```python
keys = Settings.get_section(path)
```

Return the list of keys under a given section.

```python
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
    config = Settings.get(plugin_id)
    for key in config.get_section("/modules"):
        log("Module: %s" % key)
```

#### `Settings.get_string` / `Settings.set_string`

```python
value = Settings.get_string(path, key, default_value)
Settings.set_string(path, key, value)
```

Read or write a string. Writes are not persisted until `save()` is called.

```python
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
    config = Settings.get(plugin_id)
    for key in config.get_section("/modules"):
        value = config.get_string("/modules", key, "unknown")
        log("The module %s is %s" % (key, value))
```

```python
from NSCP import Settings

def init(plugin_id, plugin_alias, script_alias):
    config = Settings.get(plugin_id)
    config.set_string("/modules", "PythonScript", "enabled")
    config.save()
```

#### `Settings.get_bool` / `Settings.set_bool`

```python
value = Settings.get_bool(path, key, default_value)
Settings.set_bool(path, key, value)
```

Read or write a boolean.

```python
from NSCP import Settings

def init(plugin_id, plugin_alias, script_alias):
    config = Settings.get(plugin_id)
    config.set_bool("/modules", "PythonScript", True)
    config.save()
```

#### `Settings.get_int` / `Settings.set_int`

```python
value = Settings.get_int(path, key, default_value)
Settings.set_int(path, key, value)
```

Read or write an integer.

```python
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
    config = Settings.get(plugin_id)
    port = config.get_int("/settings/NRPE/server", "port", -1)
    log("NRPE port is: %d" % port)
    config.set_int("/settings/NRPE/server", "port", 1234)
    config.save()
```

#### `Settings.save`

```python
Settings.save()
```

Persist any in-memory changes back to the settings store (file or registry, depending on configuration).

#### `Settings.register_path`

```python
Settings.register_path(path, title, description)
```

Register a settings section so it is documented in the generated settings file and visible in the
WEB UI.

#### `Settings.register_key`

```python
Settings.register_key(path, key, type, title, description, default_value)
```

Register an individual settings key for documentation and UI purposes. The `type` argument is
accepted for forward-compatibility, but the underlying store currently treats all script-registered
keys as strings.

#### `Settings.query`

```python
(return_code, response_bytes) = Settings.query(request_bytes)
```

Issue a raw settings query. `request_bytes` is a serialized `SettingsRequestMessage`; the response is
a `SettingsResponseMessage`. Use this when you need operations that the typed helpers above don't
cover (bulk reads, diffs, deletions, etc.).

```python
from NSCP import Settings
import plugin_pb2

def init(plugin_id, plugin_alias, script_alias):
    config = Settings.get(plugin_id)
    request_message = plugin_pb2.SettingsRequestMessage()
    # ... populate request_message ...
    (ret, response) = config.query(request_message.SerializeToString())
    response_message = plugin_pb2.SettingsResponseMessage()
    response_message.ParseFromString(response)
```
