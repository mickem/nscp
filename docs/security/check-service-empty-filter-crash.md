---
title: "A filter that matched nothing crashed the agent"
fixed_in: next
severity: "High"
modules: [CheckSystem, CheckLogFile, filters]
action: none
---
`check_service` on Windows terminated the whole `nscp` process — no result, no
crash report, Windows logging exception code `0xC0000374` (heap corruption) —
whenever its `filter` expression matched no service. `check_service
"filter=name = 'nosuchservice'"` was enough, and so was a filter that merely
missed: `=` is case sensitive, so `filter=name = 'spooler'` did it too on a
host whose service is named `Spooler` (#1499).

When nothing matches the filter, the filter framework re-evaluates the
warning and critical expressions with no object bound to the evaluation
context, so that an expression which also reads the summary (`… or count = 0`)
still reaches a verdict. `check_service` defaults to `not state_is_perfect()`
and `not state_is_ok()`, and both read the service straight off the context
without checking that one is there. That dereferenced an empty optional,
resurrecting a destroyed `shared_ptr` control block out of the vacated storage,
and the copy taken of it wrote to freed heap memory. `check_logfile`'s
`column()` keyword had the same unguarded access, reachable the same way.

The impact is denial of service: the agent dies on every check interval that
carries such a filter, which is exactly the polling a monitoring server does
when it watches for a service that is absent on some hosts. Any host past
`allowed hosts` could trigger it over NRPE with `allow arguments = true`, and
any authenticated REST client could. We have no indication the corruption is
usable for anything beyond terminating the process, but it is a write to freed
memory and was treated accordingly.

Both keywords now report an unresolved value when no object is bound, as the
built-in filter keywords already did, and the check returns its documented
empty-result contract (`UNKNOWN: No services found`). The accessor underneath
them throws a filter error instead of reading the vacated storage, so a keyword
that misses the guard in future is reported rather than corrupting the heap.

**What to do:** nothing beyond upgrading. Any `service=` and `crit=` workaround
adopted to avoid the crash can go back to using `filter=`.
