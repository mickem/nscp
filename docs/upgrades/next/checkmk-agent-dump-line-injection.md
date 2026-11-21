---
icon: "🔒"
modules: [CheckMKServer, CheckSystem, CheckSystemUnix, CheckDisk]
action: none
---
**Checkmk agent dump: line breaks in command lines, mountpoints and service names are replaced with a space.**
The `fetch-only` rows behind the `<<<ps>>>`, `<<<df>>>` and `<<<services>>>`
sections, and the service names and perfdata of `<<<local>>>` and `<<<mrpe>>>`
lines, now have every control character (`\r`, `\n`, tab, NUL, ...) replaced
with a single space; ordinary spaces are kept as they are. Before, a line
break in a process command line could end the row early and let the rest pose
as another row or a whole `<<<section>>>`. Nothing
to configure. A command line or name that contained such a character shows a
space there now.

Custom Checkmk Lua scripts: `s:add_line("...")` with a string that contains
line breaks still sends one row per line, as before (empty lines are dropped).
A line *object* filled with `line:set_line(...)` is a single row, though: a
line break inside it is now sent as a space instead of starting a new row. If
a script relied on that, call `s:add_line()` once per row. See the
[security notice](../security/notices.md#checkmk-agent-dump-a-field-taken-from-the-os-could-forge-rows-and-sections).
