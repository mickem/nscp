---
title: "Second undefined-behaviour sweep: crashes in the script hosts and the Windows collectors"
fixed_in: next
severity: "Medium"
modules: [LUAScript, PythonScript, CheckMKServer, CheckMKClient, CheckSystem, CheckEventLog]
action: conditional
---
The C++ tree was read again after the [audit that shipped in
0.20.0](notices.md#undefined-behaviour-audit-crash-and-memory-safety-fixes-across-the-agent),
and the crashes it still held were fixed. None is reachable by an
unauthenticated peer; each needs either a script the operator installed or a
configuration value they set.

- A Lua script's error text was used as the format string for the error it
  raised, so a `%` in anything the script passed in - a channel or command
  name quoted back at it - read a wild pointer and took the agent down.
- A non-numeric argument where a Lua binding expected a number threw a C++
  exception through Lua's C frames, which is undefined and terminates the
  process on Windows. `Settings():get_int(path, key, "n/a")` was enough.
- A Python script returning a string Python itself cannot encode as UTF-8 - a
  lone surrogate, which is how Python hands back a file name that is not valid
  UTF-8 - crashed the agent as its result was converted.
- A Python script that exists but cannot be opened was run through a NULL file
  handle, crashing inside the interpreter's tokenizer.
- Any agent with a Python script configured crashed as it started: the
  interpreter was initialised after the scripts were loaded.
- A Windows performance counter with `collection strategy = rrd` and a
  `buffer size` of zero (or one that does not parse) built a buffer that holds
  nothing, and the collector thread crashed a second later reading it.
- `check_eventlog` closed event handles that the object reading them already
  owned. Handle values are recycled, so the second close could land on a
  handle the real-time thread or another check was using.

None is known to have been exploited, and none is known to do more than crash
the agent.

**What to do:** nothing beyond upgrading, unless a counter of yours sets
`buffer size = 0`; see the [upgrade note](../setup/upgrading.md#unreleased).
