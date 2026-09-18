---
icon: "🔒 🔧"
modules: [core, filters, CheckHelpers, CheckLogFile]
action: conditional
---
**Three new limits on what one request may cost.** Nothing to do on a default
install; each is far above anything a real check does.

| Limit | Value | What happens past it |
|-------|-------|----------------------|
| Command nesting depth | 16 | The query is refused with a structured error. A check that runs other checks (`check_multi`, `check_and_forward`, `check_timeout`) re-enters the core on the same thread, and an unbounded nesting exhausted the thread stack — which is not a catchable failure on Windows. |
| `check_multi` commands per call | 128 | The check returns UNKNOWN naming the count. |
| Regular-expression matching per check | 30 s total, 1 MiB per subject | Further matches are refused and reported, so the check comes back UNKNOWN rather than quietly under-matching. A pattern that backtracks catastrophically no longer pins a worker thread for the length of a large event log. Compiled patterns are now cached, so ordinary filters get faster. |

`check_logfile` also gained a `max-size` option, default `64m`, bounding what
one call reads into memory:

* **With a bookmark** nothing is lost — the check consumes up to that much per
  run, the position advances over what it read, and the next run continues, so
  a large backlog is worked through over several checks.
* **Without a bookmark** there is no position to resume from, so a file over the
  limit is reported as UNKNOWN rather than silently half-read. Add
  `bookmark=auto` (which is what you want anyway: it reports each line once),
  add `max-lines`, or raise `max-size`.

See the
[security notice](../security/notices.md#resource-limits-on-command-nesting-regular-expression-matching-and-log-file-reads).
