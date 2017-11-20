# Python Scripts

In addition to executing Python script as regular script NSClient++ can also execute them via the PythonScript module inside NSClient++.
The benefit of this is that you can call APIs of NSClient++ as well as maintain state.

To add a python script as an internal script you need to load the PythonScript module as well as configure it to run your script:

There is a command line interface to do this:

```
nscp py add --script my_script.py
```

This will create the following configuration for you:

```
[/modules]
PythonScript = enabled

[/settings/python/scripts]
my_script = my_script.py
```

## Functions

The skeleton of a script is the following three functions:

 - \_\_main\_\_
 - init
 - shutdown

### \_\_main\_\_

The mainfunction is used when the script is run from the command line.
For instance when you want to provide an install or helper function to the administrator much like we did when we installed the script:

```
nscp py execute --script my_script.py install --root /tmp
```

The main function takes as input argument a list of command line options:

```
from NSCP import log
def __main__(args):
    log("My arguments are: %s"%args)
```

Running this script:

```
nscp py execute --script my_script.py install --root /tmp
L     python My arguments are: ['install', '--root', '/tmp']
```

### init

The init function called when the script is loaded and provides important variables for talking to NSClient++.
The function takes tree arguments:

- plugin_id
- plugin_alias
- script_alias

The plugin id is an internal id of the plugin instance which is used in many API calls.
The plugin and script aliases can be used to differentiate between which instance is loaded.
The idea is that you can load a script multiple times in multiple modules and they should then have seprate config.

```
from NSCP import log
def init(plugin_id, plugin_alias, script_alias):
    log("*** plugin_id: %d, plugin_alias: %s, script_alias: %s"%(plugin_id, plugin_alias, script_alias))
```

The result of running this script from NSClient++:
```
...
D     python boot python
D     python Prepare python
D     python init python
D     python Adding script: osmc (c:\source\build\x64\dev\scripts\python\osmc.py)
D     python Loading python script: c:\source\build\x64\dev\scripts\python\osmc.py
D     python Lib path: c:\source\build\x64\dev\scripts\python\lib
L     python *** plugin_id: 0, plugin_alias: python, script_alias: my_script
...
```

### plugin_alias versus script_alias

Consider the following example:

```
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

This will load the same script four times with the following arguments:

plugin_id | plugin_alias | script_alias
----------|--------------|-------------
1         | ps1          | ms1
1         | ps1          | ms2
2         | ps2          | ms1
2         | ps2          | ms2

Thus when loading configuration it is recommended to place it under the following pseudo key `/settings/:script_name/:plugin_alias/:script_alias/` to allow for users loading a script multiple times.

At the same time it is discouraged from using the alias concept to load a script multiple times which is why the command line interfaces does not provide commands for doing so.
But in complicated scenarios it can be very useful but it causes a lot of complexity.

### shutdown

The shutdown function is called whenever NSClient++ is shutting down or the PythonScript module is unloaded.
The function takes no arguments.

```
from NSCP import log
def shutdown(args):
    log("Good bye")
```

## API

NSClient++ provides a rich API where you can do just about anything that can be done with NSClient++.
This includes loading modules, accessing settings, providing command line interfaces etc etc.
While most of it has a simple wrappers which you can use directly some things might require you to use the underlying protobuf Api.

The API is split three modules.
 - Settings
 - Registry
 - Core

There is also an enum: `status` as well as a some direct functions:
 - log
 - log_error
 - log_debug
 - sleep

### Functions

#### log

`log(message)`

Used to send log messages to NSClient++.
These log messages will be visible on the command line as well as the any configure log file.
The log message will be logged on the `info` level.

```
from NSCP import log

log("This is a log message")
```

#### log_error

`log_error(message)`

Used to send log messages to NSClient++.
These log messages will be visible on the command line as well as the any configure log file.
The log message will be logged on the `error` level.

```
from NSCP import log_error

log_error("This is an error message")
```

#### log_debug

`log_debug(message)`

Used to send log messages to NSClient++.
These log messages will be visible on the command line as well as the any configure log file.
The log message will be logged on the `debug` level.

```
from NSCP import log_debug

log_debug("This is a debug message")
```

#### sleep

`sleep(milli_seconds)`

Used to sleep for a given number of milliseconds.
The reason this function exists is that python is inherently single threaded and whenever you are executing python code your are essentially locking any other code from executing in python. This means that sleeping or delaying inside python will prevent any other script to run. Thus if you need to wait please use this function as it will wait outside the locks and allow other script to run.

```
from NSCP import sleep, log_debug

log_debug("Waiting for 1 second")
sleep(1000)
log_debug("It is now one second later")
```

### Registry

#### get

Create an instance of the registry object.

#### Registry.function

`Registry.simple_function(query_name, query_function, description)`

Bind a function to a check query. This is similar to the `Registry.simple_function` function but the bound
function uses a more powerful syntax which requires you to parse the request/response using the protobuf API.

Option         | Description
---------------|----------------------------------------------------
query_name     | The name of the query (i.e. the check command name)
query_function | The function to call when the query is executed
description    | The description of the query.

The bound function should look like this:

```
def my_function(request):
  # ...
  return response_message.SerializeToString()
```

**Example:**

```
from NSCP import log, Registry, status
import plugin_pb2

def my_function(request):
  request_message = plugin_pb2.QueryRequestMessage()
  request_message.ParseFromString(request)
  log('Got command: %s'%request_message.payload[0].command)
  response_message = plugin_pb2.QueryResponseMessage()
  return response_message.SerializeToString()

def init(plugin_id, plugin_alias, script_alias):
  reg = Registry.get(plugin_id)
  reg.function('check_py_test', my_function, 'This is a sample python function')
```

#### Registry.simple_function

`Registry.simple_function(query_name, query_function, description)`

Bind a function to a check query. This is similar to the `Registry.function` function but the bound
function has a simpler syntax so you wont have to deal with the complexity of the protobuf API.


Option         | Description
---------------|----------------------------------------------------
query_name     | The name of the query (i.e. the check command name)
query_function | The function to call when the query is executed
description    | The description of th query.

The bound function should look like this:

```
def my_function(args):
  return (status.OK, "This is the messge", "'count'=123;200;600")
```

**Example:**

```
from NSCP import log, Registry, status

def my_function(args):
  log('Got arguments: %s'%args)
  return (status.OK, "This is the messge", "'count'=123;200;600")

def init(plugin_id, plugin_alias, script_alias):
  reg = Registry.get(plugin_id)
  reg.simple_function('check_py_test', my_function, 'This is a sample python function')
```

#### Registry.cmdline

`Registry.cmdline(command_name, function)`

Bind a function to a check query. This is similar to the `Registry.simple_cmdline` function but the bound
function uses a more powerfull syntax which requires you to parse the reques/response using the protobuf API.


Option       | Description
-------------|----------------------------------------------------
command_name | The name of the command to expose
function     | The function to call when the command is executed

The bound function should look like this:

```
def my_function(request):
  # ...
  return response_message.SerializeToString()
```

**Example:**

```
from NSCP import log, Registry, status
import plugin_pb2

def my_function(request):
  request_message = plugin_pb2.ExecuteRequestMessage()
  request_message.ParseFromString(request)
  log('Got command: %s'%request_message.payload[0].command)
  response_message = plugin_pb2.ExecuteResponseMessage()
  return response_message.SerializeToString()

def init(plugin_id, plugin_alias, script_alias):
  reg = Registry.get(plugin_id)
  reg.cmdline('do_foo', my_function)
```

#### Registry.simple_cmdline

`Registry.simple_cmdline(command_name, function)`

Bind a function to a check query. This is similar to the `Registry.function` function but the bound
function has a simpler syntax so you wont have to deal with the complexity of the protobuf API.

Option       | Description
-------------|----------------------------------------------------
command_name | The name of the command to expose
function     | The function to call when the command is executed

The bound function should look like this:

```
def my_function(args):
  return (0, "This is the messge")
```

**Example:**

```
from NSCP import log, Registry, status

def my_function(args):
  log('Got arguments: %s'%args)
  return (1, "Everything is awesome")

def init(plugin_id, plugin_alias, script_alias):
  reg = Registry.get(plugin_id)
  reg.simple_cmdline('do_something', my_function)
```

#### Registry.subscription

`Registry.subscription(channel, function)`

Bind a function to a notification (think passive checks results). This is similar to the `Registry.simple_subscription` function but the bound
function has a more powerful syntax with protobuf messages.

Option   | Description
---------|--------------------------------------------------
channel  | The channel name to subscribe to
function | The function to call when the command is executed

The bound function should look like this:

```
def my_function(channel, message):
  return True
```

**Example:**

```
from NSCP import Registry, Core, log_error
import plugin_pb2

def on_message(channel, message):
  message = plugin_pb2.SubmitRequestMessage()
  message.ParseFromString(request)
  if len(message.payload) != 1:
    log_error("Got invalid message on channel: %s"%channel)
    return False

def init(plugin_id, plugin_alias, script_alias):
  reg = Registry.get(plugin_id)
  reg.subscription('test-channel', on_message)
```

#### Registry.simple_subscription

`Registry.simple_subscription(channel, function)`

Bind a function to a notification (think passive checks results). This is similar to the `Registry.subscription` function but the bound
function has a simpler syntax so you wont have to deal with the complexity of the protobuf API.

Option   | Description
---------|--------------------------------------------------
channel  | The channel name to subscribe to
function | The function to call when the command is executed

The bound function should look like this:

```
def my_function(channel, source, command, status, message, perf):
  return True
```

**Example:** Filter out repeated NSCA events

```
from NSCP import Registry, Core, status, log

g_last_message = ''
g_plugin_id = 0

def filter_nsca(channel, source, command, status, message, perf):
  global g_last_message, g_plugin_id
  if not message == g_last_message:
    g_last_message = message
    log("Sending: %s"%message)
    core = Core.get(g_plugin_id)
    core.simple_submit('NSCA', command, status.CRITICAL, message.encode('utf-8'), perf)
  else:
    log("Supressing duplicte message")

def init(plugin_id, plugin_alias, script_alias):
  global g_plugin_id
  g_plugin_id = plugin_id
  reg = Registry.get(plugin_id)
  reg.simple_subscription('filter_nsca', filter_nsca)
```

To use the above specify `filter_nsca` as your target instead of NSCA like so:

```
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

#### Registry.submit_metrics

#### Registry.fetch_metrics

#### Registry.event_pb

#### Registry.event

`Registry.event(event_name, event_function)`

Register a function which listens for a given event.

Option         | Description
---------------|------------------------------------------
event_name     | The name of the event to subscribe to.
event_function | The function to call when the event fires

The bound function should look like this:

```
def my_function(event_name, data):
  log("This is from the data: %s"%data['key'])
```

**Example:**

```
from NSCP import log, Registry

def on_event(event, data):
  log('Got event: %s'%event)

  def init(plugin_id, plugin_alias, script_alias):
      reg = Registry.get(plugin_id)
      reg.event('eventlog:login', on_event)
```

#### Registry.query

### Core

The core is a representation of NSClient++ and exposes functions to interact with NSClient++ it self such as running queries or reloading modules.

#### Core.get

`Core.get()`


Get an instance of the core module.

**Example:**

```
from NSCP import Core
core = Core.get()
(code, message, perf) = core.simpler_query("check_cpu")
```

#### Core.simple_query

`(status, message, perf) = Core.simple_query(query, arguments)`

Execute a check query.

Option    | Description
----------|-------------------------------------
query     | The name of the query to execute
arguments | Arguments for the query
status    | The nagios status code of the result
message   | The resulting message
perf      | The resulting performance data

**Example:**

```
from NSCP import Core, log

core = Core.get()
(code, message, perf) = core.simpler_query("check_cpu")
log(message)
```

#### Core.query

`reply = Core.simple_query(request)`

Execute a check query.

Option  | Description
--------|--------------------------------
request | A probuf QueryRequestMessage
reply   | A protobuf QueryResponseMessage

**Example:**

```
from NSCP import Core, log
import plugin_pb2

request_message = plugin_pb2.QueryRequestMessage()
core = Core.get()
response = core.query(request_message.SerializeToString())
response_message = plugin_pb2.QueryResponseMessage()
response_message.ParseFromString(response)

log('Got command: %s'%request_message.payload[0].command)
```

#### Core.simple_exec

#### Core.exec

#### Core.simple_submit

#### Core.submit

#### Core.reload

#### Core.load_module

`Core.load_module(module, alias)`

Load a module into NSClient++.

Option | Description
-------|------------------------------------
module | The name of the module to load
alias  | A alias (if loading a module twice)

**Example:**

```
from NSCP import Core, log

def init(plugin_id, plugin_alias, script_alias):
  core = Core.get(plugin_id)
  core.load("CheckEventLog", "")
```

#### Core.unload_module

`Core.load_module(module_or_alias)`

Unload a module from NSClient++.

Option          | Description
----------------|----------------------------------------------
module_or_alias | The name or the alias of the module to unload

**Example:**

```
from NSCP import Core, log

def init(plugin_id, plugin_alias, script_alias):
  core = Core.get(plugin_id)
  core.unload("CheckEventLog", "")
```

#### Core.expand_path

`path = Core.expand_path(path_expression)`

Parse a path expression into a path.

Option          | Description
----------------|-----------------------------------------------------------------------
path_expression | A path expression which can contain ${..} keywords such as ${modules}.
path            | The resulting path

**Example:**

```
from NSCP import Core, log

def init(plugin_id, plugin_alias, script_alias):
  core = Core.get(plugin_id)
  path = core.expand_path("${scripts}/ptython/myscript.py")
  log('The script path is: %s'%path)
```

### Settings

The `Settings` object wraps the Settings API which allows you to access and modify the NSClient++ configuration file.

#### Settings.get

`Settings.get(plugin_id)`

Option    | Description
----------|------------------------------------------------------------------
plugin_id | The plugin id as supplied at module ini (i.e. the init function).


Get an instance of the settings module.

**Example:**

```
from NSCP import Settings

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  (code, message, perf) = config.get_string("/modules", "PythonScript", "disabled")
```

#### Settings.get_section

`keys = Settings.get_section(path)`

Fetch all keys under a given section in the settings file.

Option | Description
-------|----------------------------------------
path   | The settings path to query all keys for
keys   | A list of keys under a the section

**Example:**

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  for key in config.get_section("/modules"):
    log("Module: %s"%key)
```

#### Settings.get_string

`value = Settings.get_string(path, key, default_value)`

Fetch a string from the settings store given a path and a key.

Option        | Description
--------------|---------------------------------------------
path          | The settings path to query all keys for
key           | The key to lookup
default_value | The value to return if the key is not found.
value         | The resulting value.

**Example:**

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  for key in config.get_section("/modules"):
    value = config.get_string("/modules", key, "unknown")
    log("THe module %s is %s"%(key, value))
```

#### Settings.set_string

`Settings.set_string(path, key, value)`

Set a string in the settings store given a path and a key.

**please note** changing the settings will not save so unless you call `Settings.save()` afterwards the settings will never be written to your settings file.

Option | Description
-------|----------------------------------------
path   | The settings path to query all keys for
key    | The key to lookup
value  | The value to set.

**Example:**

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  config.get_string("/modules", "PythonScript", "enabled")
```

#### Settings.get_bool

`value = Settings.get_string(path, key, default_value)`

Fetch a boolean from the settings store given a path and a key.

Option        | Description
--------------|---------------------------------------------
path          | The settings path to query all keys for
key           | The key to lookup
default_value | The value to return if the key is not found.
value         | The resulting value.

**Example:**

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  for key in config.get_section("/modules"):
    value = config.get_string("/modules", key, False)
    log("THe module %s is %s"%(key, value))
```

#### Settings.set_bool

`Settings.set_bool(path, key, value)`

Set a boolean in the settings store given a path and a key.

**please note** changing the settings will not save so unless you call `Settings.save()` afterwards the settings will never be written to your settings file.

Option | Description
-------|----------------------------------------
path   | The settings path to query all keys for
key    | The key to lookup
value  | The value to set.

**Example:** Enable a module

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  config.set_bool("/modules", "PythonScript", True)
```

#### Settings.get_int

`value = Settings.get_int(path, key, default_value)`

Fetch a number from the settings store given a path and a key.

Option        | Description
--------------|---------------------------------------------
path          | The settings path to query all keys for
key           | The key to lookup
default_value | The value to return if the key is not found.
value         | The resulting value.

**Example:** Get the NRPE port

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  port = config.get_int("/settings/NRPE/server", "port", -1)
  log("NRPE port is: %d"%port)
```

#### Settings.set_int

`Settings.set_int(path, key, value)`

Set a boolean in the settings store given a path and a key.

**please note** changing the settings will not save so unless you call `Settings.save()` afterwards the settings will never be written to your settings file.

Option | Description
-------|----------------------------------------
path   | The settings path to query all keys for
key    | The key to lookup
value  | The value to set.

**Example:** Change the NRPE port.

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  config.set_int("/settings/NRPE/server", "port", 1234)
```

#### Settings.save

`Settings.save()`

Save the settings file to disk (or registry depending on where it is stored).

**Example:** Save changed settings

```
from NSCP import Settings, log

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)
  config.set_int("/settings/NRPE/server", "port", 1234)
  config.save()
```

#### Settings.register_path

Used to register a path with the settings handler.
The idea with registering keys is that this provides documentation inside the settings file and the WEB UI.

#### Settings.register_key

Used to register a key with the settings handler.
The idea with registering keys is that this provides documentation inside the settings file and the WEB UI.

#### Settings.query

`Settings.query()`

Invoke the RAW settings API with a protobuf message.

**Example:**

```
from NSCP import Settings, log
import plugin_pb2

def init(plugin_id, plugin_alias, script_alias):
  config = Settings.get(plugin_id)

  request_message = plugin_pb2.SettingsRequestMessage()
  response = config.query(request_message.SerializeToString())
  response_message = plugin_pb2.SettingsResponseMessage()
  response_message.ParseFromString(response)
```

### status

Key      | Value | Description
-------- | ----- | ----------------------------
UNKNOWN  | 3     | Unknown nagios status code
CRITICAL | 2     | Critical nagios status code
WARNING  | 1     | Warning nagios status code
OK       | 0     | Ok nagios status code
