---
title: "Checkmk agent dump: a field taken from the OS could forge rows and sections"
fixed_in: next
severity: "Medium"
modules: [CheckMKServer, CheckSystem, CheckSystemUnix, CheckDisk]
action: none
---
The Checkmk agent dump that `CheckMKServer` serves (built by
`scripts/lua/default_check_mk.lua`) wraps the `fetch-only` output of the native
checks in `<<<section>>>` headers and splits each body on line breaks. Several
fields in that output come straight from the OS and are chosen by whoever owns
the thing they describe:

- a process command line in `<<<ps>>>`: on Linux the arguments in
  `/proc/<pid>/cmdline` may contain newlines, and a Windows command line can
  carry them too;
- a file system mountpoint in `<<<df>>>`;
- a service name or display name in `<<<services>>>`.

A `\n` or `\r` in such a field ended the line early, and the text after it
became a line of its own. If that text started with `<<<name>>>`, Checkmk read
it as a new section. Anyone who could start a process on the host could
therefore plant rows or whole sections, for example an `OK` `<<<local>>>`
result, in the host's monitoring data. The service name and perfdata of
`<<<local>>>` and `<<<mrpe>>>` lines, which come from settings keys, passive
submissions and check output, were not cleaned either; only the message text
was.

Every free-text field is now cleaned before its line is assembled: each
carriage return, line feed and other control character becomes one space, and
ordinary spaces are kept exactly as they were (`include/check/check_mk_format.hpp`
for the native checks; `no_controls()` in the Lua script for the names and
perfdata of local and MRPE lines, while their message text keeps being folded
as before). A column that has to stay one word (the MRPE service name, the
device and file system of a `<<<df>>>` row, the name, state and start type of
a `<<<services>>>` row) gets a `_` instead, so the columns after it cannot
shift. The host name and OS lines of the `<<<check_mk>>>` header are cleaned
the same way.

As a backstop, the packet writer no longer lets a line break inside a line
*object*, a section title or a piggyback host name reach the wire. It does not
cover a string handed to `s:add_line()`: that is sent as one row per line, by
design, so a custom Checkmk Lua script still has to clean any field it takes
from outside before it builds a line from it.

**What to do:** nothing on a default install. Upgrade to get the fix. A field
that used to contain a control character now shows a space (or, in a one-word
column, a `_`) in its place; nothing else in it changes. If you run a custom
Checkmk Lua script, check that it does not put text from an untrusted source
into `s:add_line()` unmodified.
