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

Scripts are resolved against `${scripts}` (typically `scripts/lua/`) and may be specified with or
without the `.lua` extension. A `lib/` folder under `scripts/lua/` is added to `package.path`
automatically, so shared helpers can live in `scripts/lua/lib/`.

## Lifecycle

A Lua script can hook into three lifecycle moments:

- **Top-level code** — runs once when the script is loaded. Use this to register check commands,
  channel subscriptions, and event handlers.
- `on_start` — optional global function, invoked after every script has loaded. Use it for work that
  needs other modules to be ready.
- `main` — optional global function, invoked when the script is run from the command line.

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

The function receives the command-line arguments as an array (Lua table) and must return a
2-tuple `(code, message)`:

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
produces strings when handing values to your callbacks. Prefer the string form in your own code.

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
Returns the Nagios status string, the message, and the performance data.

```lua
local code, msg, perf = core:simple_query("check_cpu", {"warn=load > 80", "crit=load > 90"})
nscp.info(string.format("%s: %s (%s)", code, msg, perf))
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

Execute a command-line command against `target`. `target` is a remote NSClient++ instance name, or
`""` for in-process. `results` is a Lua array of result strings.

```lua
local code, lines = core:simple_exec("", "list-modules", {})
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

```lua
core:simple_submit("NSCA", "check_battery", "warning", "Battery low (15%)", "")
```

#### `Core:reload`

```lua
core:reload(module)
```

Reload the given module by name. Pass `"service"` to reload the entire service.

#### `Core:log`

```lua
core:log(level, message)
```

Log a message at the specified level. `level` is a string: `"info"`, `"error"`, `"debug"`, etc.
`nscp.info()` / `nscp.error()` are usually more convenient.

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

Register a **command-line command** invoked via `nscp ext --command <name>`. Callback signature is
`(command, args) -> (code, message)`:

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

Return `(success_bool, message)`.

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

Read or write a boolean.

#### `Settings:get_int` / `Settings:set_int`

```lua
value = config:get_int(path, key, default)
config:set_int(path, key, value)
```

Read or write an integer.

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
