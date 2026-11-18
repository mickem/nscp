---
icon: "🔒 💥"
modules: [filters, CheckNet, CheckDisk]
action: conditional
---
**An expression that fails on a record now ends the check UNKNOWN.** When a
`filter`, `warning` or `critical` expression failed while the records were
evaluated, the error only reached the debug log and the check carried on with
whatever value came back, so it reported a verdict nobody asked for:

| Expression | Before | Now |
|---|---|---|
| `critical=load > 40k` (a unit the keyword does not take) | compared against a bare `40`: CRITICAL for a load of 50 | UNKNOWN: `Filter evaluation failed: Invalid unit: k: …` |
| `critical=load != 'busy'` (text where a percentage or size keyword wants a number) | the text was read as `0`: `load != 0` | UNKNOWN: `Filter evaluation failed: Not a number: busy: …` |
| a regular expression refused by the time budget or the 1 MiB subject limit | read as *no match*: OK, or "No entries found" | UNKNOWN (see the [security notice](../security/notices.md#a-refused-regular-expression-match-no-longer-reads-as-a-clean-result)) |
| `check_ping … count=0` | nothing sent, `Packet loss = 0%`: OK | refused: `Invalid count: 0 (expected 1 or more)` |
| `check_disk_write … warning=size > 8k` | the unit was refused and `size` compared against 0, so it always fired | `size` is a byte count: `8k` is 8192 |

This applies to every check built on the shared filter framework. Errors in
setting the filter up (an unknown keyword, a syntax error) already failed the
check and are unchanged, as is the evaluation without any record (`count = 0`
and similar summary expressions). So is the cross-type rule: text that is not
a number never matches a numeric comparison (`ival = 'beta'` is false for
every row, and the `'unknown'` probe of an optional number such as
`jitter != 'unknown'` works as before) - that is a defined answer, not a
failure.

**What to do:** only if a check starts reporting `Filter evaluation failed:`
after the upgrade. Its result was not computed from the expression as written
before either; the message names the expression and why it failed - correct
the unit, the keyword or the regular expression.
