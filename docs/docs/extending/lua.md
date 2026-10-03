# Lua Scripts

NSClient++ can host Lua scripts in-process via the **LUAScript** module. Hosted scripts can call
NSClient++'s APIs directly — run checks, listen on channels, register new check commands, read and
write settings — and they keep state between invocations.

To add a Lua script as an internal script you need to load the LUAScript module and register the
script:

```
nscp lua add --script my_script.lua
```

This produces:

```ini
[/modules]
LUAScript = enabled

[/settings/lua/scripts]
my_script = my_script.lua
```

The key is the script's alias, and `add` uses the file name without `.lua` unless you pass
`--alias my_script`. Scripts are resolved against `${scripts}` (typically `scripts/lua/`) and may be
specified with or without the `.lua` extension. A `lib/` folder under `scripts/lua/` is added to
`package.path` automatically, so shared helpers can live in `scripts/lua/lib/`.

## Managing scripts

| Command                                                    | What it does                                                                 |
|------------------------------------------------------------|------------------------------------------------------------------------------|
| `nscp lua list [--json] [--include-lib]`                   | List the files under `${scripts}/lua`; helpers in a `lib` folder only with `--include-lib` |
| `nscp lua add --script <file> [--alias <name>]`            | Configure a script and enable the module; `--no-config` leaves the configuration alone |
| `nscp lua add --script <file> --import <path> [--replace]` | Copy a script into `${scripts}/lua` first; an existing file is only overwritten with `--replace` |
| `nscp lua show --script <file>`                            | Print a script                                                               |
| `nscp lua delete --script <file>`                          | Delete a script and every `[/settings/lua/scripts]` entry that loads it      |
| `nscp lua install --add <file>` / `--remove <file>`        | Add or remove a configured script                                            |
| `nscp lua execute --script <file> [args...]`               | Run a script's `main` (see below)                                            |

A script added this way loads the next time LUAScript loads or reloads. `show` and `delete` only reach
`${scripts}/lua`; a name that resolves anywhere else is refused, and so is a path through a symlinked
folder that leads out of it. `show` reads a symlink only when its target is inside the folder too.
`delete` removes the entry in the folder - a file, or a symlink itself, whatever it points at,
dangling or not - and never what a link points to. A deleted script stays loaded until the module
reloads. The same operations are available over REST under
[`/api/v2/scripts/lua`](../api/rest/scripts.md).

## Lifecycle

A Lua script can hook into three lifecycle moments:

- **Top-level code** — runs once when the script is loaded. Use this to register check commands,
  channel subscriptions, and event handlers.
- `on_start` — optional global function, invoked once every script and every module has loaded. Use
  it for work that needs other modules to be ready.
- `main` — optional global function, invoked when the script is run from the command line.

A script that does not parse, or whose top-level code raises an error, is logged and never started:
its `on_start` does not run, though whatever its top-level code registered before the error stays
registered. An `on_start` that raises is logged. Either way the other scripts load and start
regardless.

A reload of the module (`Core():reload("LUAScript")`, or a reload of the service) loads every script
afresh: the top-level code runs again in a new Lua state, so any state a script kept in its variables
starts over, and `on_start` runs again once the reload is complete - on a reload of the service, after
any module the same reload enabled has loaded. Commands and channels a script no longer registers -
because it was deleted, or changed - are gone after the reload.

### Top-level code

The body of the script runs exactly once, when the LUAScript module loads it. This is where you
register everything the script wants to expose:

```lua
local function check_hello(command, args)
    return "ok", "Hello from Lua!", ""
end

Registry():simple_query("check_hello", check_hello, "A Lua greeting")
```

After the script is loaded, NSClient++ will route `check_hello` to your function.

### `on_start`

Define a global `on_start()` function if you need a hook that fires *after* all scripts have been
loaded — e.g. to talk to other modules or schedule background work.

```lua
function on_start()
    nscp.info("Lua script ready")
end
```

`on_start` takes no arguments and does not need to return anything.

### `main`

`main` is invoked when the script is run from the command line:

```
nscp lua execute --script my_script.lua install --root /tmp
```

The script is loaded as a second copy for the run - its top-level code runs, `on_start` does not -
and `main` receives the command-line arguments as an array (Lua table). It must return a 2-tuple
`(code, message)`; a script with no `main` fails with `Failed to handle command main`:

```lua
function main(args)
    for i, v in ipairs(args) do
        nscp.info(string.format("arg %d = %s", i, v))
    end
    return "ok", "Ran with " .. #args .. " arguments"
end
```

## Status codes

Throughout the API, status codes are represented as **strings**:

| String     | Meaning           |
|------------|-------------------|
| `"ok"`     | Nagios OK         |
| `"warning"`| Nagios warning    |
| `"critical"`| Nagios critical  |
| `"unknown"`| Nagios unknown    |

The wrapper also accepts the corresponding integer codes (`0`/`1`/`2`/`3`) on input, but always
produces strings when handing values to your callbacks. Prefer the string form in your own code. Any
other value - a string not in the table, or an integer outside `0`-`3` - is read as `"unknown"`.

## Errors in handlers

A handler that raises an error does not take the agent down. The caller gets an answer it can act on,
and the error is logged:

| Handler                      | Raises an error                                            | Returns no status                                                 |
|------------------------------|------------------------------------------------------------|-------------------------------------------------------------------|
| `simple_query` (a check)     | `unknown`, `Failed to handle command: <command>: <error>`  | `unknown`, `Invalid return from <command>: expected (code, message, perf)` |
| `simple_cmdline`             | `Failed to handle command: <command>: <error>`, non-zero exit | `Invalid return from <command>: expected (code, message)`, non-zero exit |
| `simple_subscription`        | the submission fails with `Failed to handle channel: <channel>: <error>` | the submission fails                                  |

The message and the performance data are optional: a handler that returns only `"warning"` is a
warning with an empty message.

A call into the API with too few arguments raises a Lua error naming the expected syntax (for example
`Incorrect syntax: simple_query(command, args)`), which the script can catch with `pcall`.

## API

The Lua environment exposes:

- A **`nscp`** global with utility functions (`info`, `print`, `error`, `sleep`, `getSetting`)
- Three constructors that return wrapper objects: **`Core()`**, **`Registry()`**, **`Settings()`**
- Standard Lua library functions (`string`, `table`, `io`, `os`, etc.)

Wrapper objects use **method-call syntax** (`obj:method(args)`). The `:` is required — calling with
`.` will not pass the object instance correctly.

```lua
local core = Core()
local code, msg, perf = core:simple_query("check_cpu", {})
```

Most operations have a **simple** variant that uses plain Lua values, and a **raw** variant that
takes serialized protobuf messages. Use the simple form unless you need fields it doesn't expose.

### The `nscp` global

#### `nscp.info` / `nscp.print` / `nscp.error`

```lua
nscp.info(message)
nscp.print(message)   -- alias for nscp.info
nscp.error(message)
```

Write a message to the NSClient++ log. `info` and `print` log at the info level; `error` logs at the
error level.

```lua
nscp.info("Script starting up")
nscp.error("Something is wrong: " .. err)
```

#### `nscp.sleep`

```lua
nscp.sleep(milliseconds)
```

Sleep for the given number of milliseconds. Use this rather than busy-looping or shelling out to
`os.execute("sleep ...")`.

```lua
nscp.info("Waiting 500ms")
nscp.sleep(500)
```

#### `nscp.getSetting`

```lua
value = nscp.getSetting(path, key, default)
```

One-shot helper for reading a single string setting without instantiating `Settings()`.

```lua
local port = nscp.getSetting("/settings/NRPE/server", "port", "5666")
```

For anything more involved, instantiate `Settings()` directly (see below).

### Core

`Core()` returns a wrapper around the running NSClient++ instance — use it to run check commands,
submit passive results, and reload modules.

```lua
local core = Core()
```

#### `Core:simple_query`

```lua
code, message, perf = core:simple_query(command, args)
```

Run a check command. `args` can be a Lua table of argument strings or a single argument string.
Returns the Nagios status string, the message, and the performance data. A command nobody registered
returns `"unknown"` and `Unknown command(s): <command>`.

```lua
local code, msg, perf = core:simple_query("check_cpu", {"warn=load > 80", "crit=load > 90"})
nscp.info(string.format("%s: %s (%s)", code, msg, perf))
```

#### `Core:query_target`

```lua
code, message, perf = core:query_target(target, command, args)
```

Like `simple_query`, but the request names `target` - one configured for a client module, such as
`[/settings/NRPE/client/targets/<target>]` - in its header. A client module's query command that is
given no `target=` of its own, such as `nrpe_query`, runs on the target named there. `args` can be a
Lua table or a single string.

```lua
local code, msg = Core():query_target("backup-server", "nrpe_query", {"command=check_cpu"})
```

Only a command that takes its target from the request does anything with it: any other command - one a
module or script on this agent serves - runs here as `simple_query` would, whatever `target` says. To
run an arbitrary command on a remote agent, use `query_forward`.

#### `Core:query_forward`

```lua
code, message, perf = core:query_forward(forward_command, target, command, args)
```

Relay a command to a target through a client module's forwarding command (`nrpe_forward` for NRPE).
Unlike `query_target`, the command and its arguments are sent on the wire exactly as given, so the
remote agent receives them unchanged. Returns the remote answer.

```lua
local code, msg, perf = Core():query_forward("nrpe_forward", "backup-server", "check_drivesize", {"drive=C:"})
```

#### `Core:query`

```lua
ok, response_bytes = core:query(request_bytes)
```

Raw protobuf variant of `simple_query`. `request_bytes` is a serialized `QueryRequestMessage`;
`response_bytes` is a serialized `QueryResponseMessage`. Use `Core:create_pb_query` to build the
request bytes from a command and argument list.

#### `Core:create_pb_query`

```lua
request_bytes = core:create_pb_query(command, args)
```

Build a serialized `QueryRequestMessage` for `Core:query`. `args` can be a Lua table or a single
string.

```lua
local req = core:create_pb_query("check_cpu", {"warn=load > 80"})
local ok, resp = core:query(req)
```

#### `Core:simple_exec`

```lua
code, results = core:simple_exec(target, command, args)
```

Execute a command-line command - one a script registered with `Registry:simple_cmdline`, or one a
module provides. `args` is a Lua table. `target` picks the module to run it in:

| `target`               | Runs the command in                         |
|------------------------|---------------------------------------------|
| a module name or alias | that module (`"LUAScript"`, `"CheckSystem"`) |
| `"any"`                | the first module that has the command       |
| `"all"` or `"*"`       | every module that has the command           |

The name is matched as a substring of each module's name. Avoid `""`: it matches every module as
though each had been named. `code` is the status string the command answered with, and `results` a
Lua array of strings, one per module that answered. When nothing could run it - no such module, or no
such command in it - `code` is `"unknown"` and `results` holds the reason,
`Failed to execute <command> on <target>`.

```lua
local code, lines = core:simple_exec("LUAScript", "say_hello", {"world"})
for _, line in ipairs(lines) do nscp.info(line) end
```

#### `Core:simple_submit`

```lua
ok, response = core:simple_submit(channel, command, code, message, perf)
```

Submit a passive check result on a channel (e.g. `"NSCA"`, `"NRDP"`).

| Argument  | Description                                      |
|-----------|--------------------------------------------------|
| `channel` | Channel to submit to                             |
| `command` | Check command name being reported                |
| `code`    | Status string (`"ok"`, `"warning"`, ...)         |
| `message` | Message text                                     |
| `perf`    | Performance data string                          |

`ok` is `true` when the channel took the result and `response` is what it answered. A channel nobody
listens on returns `false` and `Failed to submit message: <channel>`.

```lua
core:simple_submit("NSCA", "check_battery", "warning", "Battery low (15%)", "")
```

#### `Core:reload`

```lua
core:reload(module)
```

Reload the given module by name. Pass `"service"` to reload the entire service. A script that asks
for its own module (`"LUAScript"`) to be reloaded gets its answer first: the core runs the reload once
the call has returned, and the script then starts over as described under [Lifecycle](#lifecycle).

#### `Core:log`

```lua
core:log(level, message)
```

Log a message at the specified level. `level` is a string: `"info"`, `"error"`, `"debug"`, etc. The
line is attributed to the script and the line that called `log`. `nscp.info()` / `nscp.error()` are
usually more convenient.

#### Not implemented

`Core:exec` and `Core:submit`, the raw protobuf variants of `simple_exec` and `simple_submit`, are not
implemented: calling one raises the Lua error `Unsupported API called: Core:exec` (or `Core:submit`).
Use the simple variants.

### Registry

`Registry()` lets a script publish itself into NSClient++ — registering check commands, command-line
commands, and channel subscriptions.

```lua
local reg = Registry()
```

Registration happens at the top level of the script; once registered, your callbacks fire whenever
the corresponding command is invoked.

#### `Registry:simple_query` / `Registry:simple_function`

```lua
reg:simple_query(name, function, description)
reg:simple_function(name, function, description)   -- alias
```

Register a **check command** with a simple callback. Both names refer to the same registration
helper — `simple_query` reads more naturally for queries.

| Argument      | Description                                       |
|---------------|---------------------------------------------------|
| `name`        | The check command name (e.g. `check_hello`)       |
| `function`    | Callback invoked when the command runs            |
| `description` | Help text shown by `<command> help` and the WEB UI |

The callback signature is `(command, args) -> (code, message, perf)`:

```lua
local function check_hello(command, args)
    return "ok", "Hello!", "'count'=1;5;10"
end

Registry():simple_query("check_hello", check_hello, "Returns a greeting")
```

#### `Registry:query`

```lua
reg:query(name, function, description)
```

Register a check command with a **raw, protobuf-based** callback. Use `simple_query` instead unless
you need fields it doesn't expose.

The callback signature is `(command, request_bytes, request_message_bytes) -> response_bytes`,
where `response_bytes` is a serialized `QueryResponseMessage`.

#### `Registry:simple_cmdline`

```lua
reg:simple_cmdline(name, function, description)
```

Register a **command-line command**, run with `nscp client --module LUAScript --exec <name> [args...]`
or from a script with `Core:simple_exec`. Callback signature is `(command, args) -> (code, message)`.

The module's own verbs come first, so a handler registered under one of their names is never reached:
`help`, `execute`, `lua-script`, `lua-run`, `add`, `install`, `list`, `show` and `delete`.

```lua
local function do_something(command, args)
    return "ok", "Did " .. (args[1] or "nothing")
end

Registry():simple_cmdline("do_something", do_something, "Sample cmdline command")
```

#### `Registry:simple_subscription`

```lua
reg:simple_subscription(channel, function, description)
```

Subscribe to a **channel** (think passive check submissions). Per submission, the callback receives
the channel, the originating command name, the status code, and a Lua table of `{message = perf}`
lines:

```lua
local function on_submit(channel, command, code, lines)
    for msg, perf in pairs(lines) do
        nscp.info(string.format("%s on %s [%s]: %s", command, channel, code, msg))
    end
    return true, "ok"
end

Registry():simple_subscription("MY-CHANNEL", on_submit, "Custom submission handler")
```

Return `(success_bool, message)`. Returning `false` fails the submission.

To route real-time submissions through your handler, point a filter or client at the channel name
you registered:

```ini
[/modules]
LUAScript=enabled
CheckEventLog=enabled

[/settings/eventlog/real-time/filters/login]
log=Security
filter=id=4624
target=MY-CHANNEL
```

#### `Registry:cmdline` / `Registry:subscription`

The raw protobuf variants of `simple_cmdline` and `simple_subscription` are not implemented: calling
one raises the Lua error `Unsupported API called: Registry:cmdline` (or `Registry:subscription`).

### Settings

`Settings()` wraps the configuration store. Reads return current values; writes are **in-memory only**
until `:save()` is called.

```lua
local config = Settings()
```

#### `Settings:get_section`

```lua
keys = config:get_section(path)
```

Return the keys under a given section as a Lua array of strings.

```lua
for _, key in ipairs(Settings():get_section("/modules")) do
    nscp.info("Module: " .. key)
end
```

#### `Settings:get_string` / `Settings:set_string`

```lua
value = config:get_string(path, key, default)
config:set_string(path, key, value)
```

Read or write a string. Writes are not persisted until `:save()` is called.

```lua
local config = Settings()
local existing = config:get_string("/modules", "LUAScript", "disabled")
config:set_string("/modules", "LUAScript", "enabled")
config:save()
```

#### `Settings:get_bool` / `Settings:set_bool`

```lua
value = config:get_bool(path, key, default)
config:set_bool(path, key, value)
```

Read or write a boolean. `true`, `1` and `yes` read as `true`; any other value reads as `false`, and
`default` is returned only when the key is not set.

#### `Settings:get_int` / `Settings:set_int`

```lua
value = config:get_int(path, key, default)
config:set_int(path, key, value)
```

Read or write an integer. A value that is not a number reads as `default`.

```lua
local port = Settings():get_int("/settings/NRPE/server", "port", 5666)
nscp.info("NRPE port is: " .. port)
```

#### `Settings:save`

```lua
config:save()
```

Persist any in-memory changes back to the settings store.

#### `Settings:register_path`

```lua
config:register_path(path, title, description)
```

Register a settings section for documentation and WEB UI purposes.

#### `Settings:register_key`

```lua
config:register_key(path, key, type, title, description, default)
```

Register an individual settings key. `type` is a type hint string (`"string"`, `"int"`, `"bool"`).

```lua
local config = Settings()
config:register_path("/settings/my_script", "My Lua script", "Configuration for my Lua script")
config:register_key("/settings/my_script", "interval", "int",
                    "Sampling interval",
                    "How often, in seconds, to sample",
                    "60")
```

## A complete example

A script that registers two checks and a passive submission handler:

```lua
-- scripts/lua/example.lua

-- A simple active check
local function check_random(command, args)
    local n = math.random(0, 100)
    if n > 90 then
        return "critical", "Random value " .. n .. " is too high", "'random'=" .. n .. ";80;90"
    elseif n > 80 then
        return "warning", "Random value " .. n .. " is high", "'random'=" .. n .. ";80;90"
    else
        return "ok", "Random value " .. n .. " is fine", "'random'=" .. n .. ";80;90"
    end
end

-- A passive submission handler that logs every submission
local function on_submit(channel, command, code, lines)
    for msg, _ in pairs(lines) do
        nscp.info(string.format("[%s] %s %s: %s", channel, command, code, msg))
    end
    return true, "ok"
end

-- A cmdline command for ad-hoc invocation
local function say_hello(command, args)
    return "ok", "Hello, " .. (args[1] or "world")
end

local reg = Registry()
reg:simple_query("check_random", check_random, "Random-number sanity check")
reg:simple_subscription("LOG-SUBMIT", on_submit, "Log every submitted result")
reg:simple_cmdline("say_hello", say_hello, "Greet someone")

function on_start()
    nscp.info("example.lua ready")
end

function main(args)
    nscp.info("Invoked from the command line with " .. #args .. " arguments")
    return "ok", "done"
end
```

Enable it via:

```ini
[/modules]
LUAScript = enabled

[/settings/lua/scripts]
example = example.lua
```
