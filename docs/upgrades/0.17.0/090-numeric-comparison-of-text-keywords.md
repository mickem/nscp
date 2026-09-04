---
icon: "🔢"
modules: [filters, CheckHelpers, CheckSystem, CheckSystemUnix, CheckLogFile]
action: conditional
---
**Filter comparisons between a text keyword and a bare number are now
numeric.** A string-typed keyword compared against an unquoted number used
to order *lexically* — `filter=value > 90` on `filter_perf` matched
`value=100` as false ("100" sorts before "90") — or, with the operands
reversed (`90 > value`), failed to evaluate at all. Both now compare as
numbers, whichever side the keyword is on: the row's text is parsed per
record, and a value that is not a number simply never matches (the check
logs one warning naming the value; the result stays a certain
non-match, not UNKNOWN). This applies to keywords such as
`value`/`warn`/`crit`/`min`/`max` (`filter_perf`, `render_perf`),
`speed` (`check_network`), `string_value` (`check_registry_value`) and the
`column()` function (`check_logfile`). **Quoted** literals keep the lexical
comparison — `version < '8'` still orders as text — as do `like`, `regexp`
and `in`, keyword-specific converters (`state = 'running'`,
`age > 30m`), and the `= 'unknown'` / `= 'never'` sentinels for optional
values. Review any filter that deliberately relied on text ordering
against a bare number: quote the number to keep the old behaviour.
