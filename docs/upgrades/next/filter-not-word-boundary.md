---
icon: "🔧"
modules: [filters]
action: none
---
**Filter keywords that start with "not" work in every position.** The filter
language matched `not` as a prefix rather than as a word, so a keyword
beginning with it - a CSV column such as `NotAfter` or `notes`, a WMI or
log-file field named that way - was read as `not` followed by the rest:
`NotAfter = '2026-01-01'` became `not (After = '2026-01-01')`, and the check
answered UNKNOWN with "expression is not valid". `not` now has to stand on its
own (`not x = 1`, `not(x = 1)`), and the built-in `not_after`, `not_before`
and `not_yet_valid` keywords are unaffected. Nothing to change.
