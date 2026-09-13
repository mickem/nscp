---
icon: "🔧"
modules: [CheckLogFile]
action: conditional
---
**`check_logfile files=` now does what it says.** The comma-separated form of
`file=` was parsed before the check read its arguments, so it has been ignored
for as long as anyone is likely to have tried it: a check passing only `files=`
failed with *Need to specify at least one file*, and one passing both `file=`
and `files=` silently read only the `file=` entries. Both now work, and the two are one list rather than one
overriding the other. If a check of yours passes both, it will read the union
from this release on - and every name goes through `file access` in
`[/settings/logfile]` exactly like a `file=` one. Nothing to do unless you were
relying on the entries being dropped.
