---
icon: "📊"
modules: [CheckNet]
action: conditional
---
**`check_http` compares `json-path` values as decimals, and a missing value is
"no value" rather than 0.** A whole-number threshold used to compare the
extracted value as an integer, so a queue length of `100.5` was truncated to
`100` and `crit=qlen > 100` stayed OK; the perfdata was truncated the same way.
Every numeric comparison is now made on the decimal value and the metric keeps
its fraction. A path that is missing from the body, holds `null`, a string, an
array or an object - or a body that is not JSON at all - used to read as `0`:
`warn=qlen < 5` fired on it and a `0` was plotted. It now matches no numeric
comparison and emits no metric; it still renders (and compares as a string) as
its text, so `st = 'degraded'` works as before, and a missing or `null` path
renders as `missing`. If you relied on a missing value counting as `0` (for
example `crit=qlen < 1` to catch a vanished field), test for it instead:
`crit=qlen = 'missing'`.
