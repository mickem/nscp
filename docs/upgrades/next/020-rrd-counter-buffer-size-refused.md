---
icon: "🔒 🔧"
modules: [CheckSystem, LUAScript, PythonScript, CheckEventLog]
action: conditional
---
**A round-robin counter with a zero buffer size is now refused.** From the
[security notice](../security/notices.md#second-undefined-behaviour-sweep-crashes-in-the-script-hosts-and-the-windows-collectors);
nothing to do unless you have one of these:

- A Windows counter with `collection strategy = rrd` and `buffer size = 0`, or
  a `buffer size` that does not parse, is skipped and named in the log instead
  of being loaded with a buffer that holds nothing. Remove the setting to get
  the 60m default, or set a real window.
- A Lua binding given a word where it expects a number reads it as `0` rather
  than terminating the agent. `Settings():get_int(path, key, "n/a")` returns
  the key's value with `0` as the default, where it used to be fatal.
- A Python string that cannot be encoded as UTF-8 comes back with the
  offending characters replaced instead of crashing the agent.
