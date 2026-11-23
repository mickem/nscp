---
icon: "🔎"
modules: [filters]
action: none
---
**`show-query` prints the query a check would run, instead of running it.**
Every filter check takes the new common option `show-query` (`show-query=true`
over REST). Given any other arguments, the check prints the query it would
evaluate with them, as `"key=value"` tokens in the shape of `show-default`:
`-w` and `-c` resolved into `warning` and `critical`, one token per
expression, the syntaxes, and the check's own options, all with their final
values. It also shows what a check changes on its own before it runs: the
filter `check_cpu cores` removes, the clauses `check_service only-essential`
adds, a unique index `check_eventlog unique` sets. The expressions are still
parsed, so an invalid one is reported as on a real run. `check_x show-query`
with no other arguments shows the defaults in their resolved form.
`show-default` is unchanged. See
[Seeing what a check runs](../concepts/checks.md#seeing-what-a-check-runs-show-query).
Nothing to do: the only new name is the option `show-query`, which no check
used.
