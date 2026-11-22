---
icon: "🔒 💥"
modules: [filters, CheckDisk, CheckSystem, CheckSystemUnix, CheckNet, CheckHyperV, CheckWindowsApps, CheckTaskSched, CheckLogFile, CheckEventLog]
action: conditional
---
**An expression that fails while a check runs is now reported, not
evaluated on a substitute.** When a `filter`, `warning` or `critical`
expression, or a function in a syntax template, failed while the check was
evaluated, the error only reached the debug log and the check carried on with
whatever value came back, so it reported a verdict nobody asked for. Now the
record it failed on gets no verdict at all - it is neither matched nor
flagged - and the check says so:

| Expression | Before | Now |
|---|---|---|
| `critical=load > 40k` (a unit the keyword does not take) | compared against a bare `40`: CRITICAL for a load of 50 | UNKNOWN: `Filter evaluation failed: Invalid unit: k (in: load > 40k)` |
| `critical=load != 'busy'` (text where a percentage or size keyword wants a number) | the text was read as `0`: `load != 0` | UNKNOWN: `Filter evaluation failed: Not a number: busy (in: …)` |
| `critical=count > 5k` (a summary keyword, evaluated once after the records) | compared against 0: CRITICAL | UNKNOWN |
| a regular expression refused by the time budget or the 1 MiB subject limit | read as *no match*: OK, or "No entries found" | UNKNOWN (see the [security notice](../security/notices.md#a-refused-regular-expression-match-no-longer-reads-as-a-clean-result)) |
| `detail-syntax=%(format_bytes(size, 'ZB'))` (a function in a template that fails) | an empty placeholder in an otherwise clean result | UNKNOWN |
| `check_ping … count=0` | nothing sent, `Packet loss = 0%`: OK | refused: `Invalid count: 0 (expected 1 or more)` |

Where the records that did evaluate reached WARNING or CRITICAL, that state
stands and the failure is appended to the message
(`CRITICAL: b=20 (Filter evaluation failed: …)`), so a real alert is not
downgraded. That includes the record that failed: one whose `critical`
failed but whose `warning` holds is a WARNING. Anything else - OK, or
nothing matched - could be the failure hiding a problem, and the check is
UNKNOWN.

The message names the error and the expression as you wrote it, on one line
of at most a few hundred characters. It never repeats the values of the
record that failed: those stay in the debug log.

Real-time filters (`CheckLogFile` and `CheckEventLog` with `real-time` set)
follow the same rule over each batch (for a log file, every line written since
the last read). A filter that submits a status keeps the WARNING or CRITICAL
its matched records reached, with the failure beside it, and submits UNKNOWN
otherwise. One that feeds an event channel still emits every record that
matched; the failure, which an event cannot carry, is logged as an error and
counted on the filter. A failure on its own does not start the `silent
period`, so a matching line right after a failing one still alerts. A filter
that keeps failing - every batch, which for the CheckSystem filters is every
second - is counted every time but reported (logged, and submitted as
UNKNOWN) when the failure is new and then once per silent period, or every
five minutes when none is set.

Some keywords that hold seconds or bytes could not take a unit at all, so a
threshold written the way their own descriptions, defaults or the documented
samples write it was compared against 0. They now take one; a plain number
keeps meaning seconds or bytes:

| Check | Keywords | Now takes |
|---|---|---|
| `check_shadowcopy` | `newest` | durations - its own defaults `newest > 26h` / `newest > 50h` fired on every volume before |
| `check_shadowcopy` | `used`, `allocated`, `max_size` | byte units |
| `check_files`, `check_single_file` | `age` | durations (`age > 7d`) |
| `check_registry_key`, `check_registry_value` | `age` | durations |
| `check_drivesize` | `trend_span` | durations (`trend_span < 1h`) |
| `check_service` (Linux, macOS) | `age` / `rss`, `vms` | durations / byte units (`rss > 1G`) |
| `check_uncpath` | `size`, `free`, `used`, `user_free` | byte units (`free < 100g`) |
| `check_storagepool` | `capacity`, `used`, `free` | byte units |
| `check_disk_write` | `size` | byte units |
| `check_hyperv_vms`, `check_iis_app_pools`, `check_iis_sites`, `check_apache_status` | `uptime` | durations |
| `check_tasksched` | `last_run_age` | durations (`last_run_age > 1d`) |

On `check_files`, a file without a version resource renders `${version}` as
empty, as before; it is not an error.

This applies to every check built on the shared filter framework. Errors in
setting the filter up (an unknown keyword, a syntax error) already failed the
check and are unchanged, as is the evaluation without any record that lets
`count = 0` and similar mixed expressions reach a verdict. So is the
cross-type rule: text that is not a number never matches a numeric comparison
(`ival = 'beta'` is false for every row, and the `'unknown'` probe of an
optional number such as `jitter != 'unknown'` works as before). That is a
defined answer, not a failure.

**What to do:** only if a check starts reporting `Filter evaluation failed:`
after the upgrade. Its result was not computed from the expression as written
before either: correct the unit, the keyword or the regular expression the
message names. A threshold that used a unit on one of the keywords above
compared against 0 before, so it may now stop firing where it always fired:
check that it says what you meant.
