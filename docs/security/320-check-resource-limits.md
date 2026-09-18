---
title: "Resource limits on command nesting, regular-expression matching and log-file reads"
fixed_in: next
severity: "Medium: denial of service reachable by any caller allowed to pass check arguments"
modules: [core, filters, CheckHelpers, CheckLogFile]
action: conditional
---
Three unbounded costs, each reachable from one request by a caller that is
allowed to pass arguments to a check — an NRPE peer with
`allow arguments = true`, or an authenticated REST caller with
`queries.execute`.

#### Command nesting

A check that runs other checks — `check_multi`, `check_and_forward`,
`check_timeout`, the Scheduler's on-demand runs — calls back into the core on
the same OS thread, and nothing bounded that recursion. One argument string
nesting `check_multi` a few thousand levels deep pushed a few thousand full
handler frames (protobuf messages, an `options_description`, the filter
machinery) and exhausted the thread stack. Stack exhaustion is not a catchable
exception on Windows, so the process died; there is no recovering from it after
the fact.

Query dispatch now carries a per-thread depth counter and refuses past 16
levels with a structured error. A wrapper around a wrapper is depth 2 or 3, so
the limit is far above anything real. `check_multi` separately caps its
fan-out at 128 commands per call: the depth guard stops the recursion going
deep, this stops one request going wide.

#### Regular-expression backtracking

Both halves of a `=~` are untrusted. The pattern comes from a `filter`,
`warning` or `critical` argument; the subject is what the check is looking at —
a log line, a process name, an event-log string — which a local unprivileged
user can usually plant. A pattern like `(a+)+$` against a long non-matching
subject backtracks exponentially.

Boost.Regex has always had a state-count ceiling, so a single match could not
run forever, but it could burn on the order of a second of CPU — and a filter
is evaluated once per record, so over a large event log or a multi-gigabyte
file that is minutes of a pinned worker thread for one request. Worse, the
exception Boost throws on that ceiling was reported as "Invalid syntax in
regular expression", sending operators to look for a typo in a pattern that is
perfectly well formed.

The filter engine now keeps a per-check budget for the total time spent inside
regex matching, across every record of one check. Past the budget further
matches are refused and reported, so the check comes back UNKNOWN rather than
quietly under-matching. A subject longer than 1 MiB is refused rather than
truncated, because truncating would silently change the verdict. Compiled
patterns are cached per thread, which removes a per-record compile that was
pure waste. And a complexity failure is now reported as what it is, with advice
on what to change.

#### Whole-file log reads

`check_logfile` with the default `max-lines=0` and no bookmark read the entire
target into one string and matched every record of it. Pointed at a large file
— which the caller names — one call inflated the agent to the size of the file
and did file-sized matching work.

A new `max-size` option, default 64 MiB, bounds what one call reads. With a
bookmark the limit is pacing and nothing is lost: the check consumes up to that
much, the stored position advances over what it read at a record boundary, and
the next check continues. Without a bookmark there is no position to resume
from, so a file over the limit is reported as UNKNOWN naming the three ways out
rather than being silently half-read.

**What to do:** nothing on a default install. If a `check_logfile` command
reads a file larger than 64 MiB in one go, add `bookmark=auto` (which is what
you want anyway — it reports each line once), add `max-lines`, or raise
`max-size`. If a `check_multi` genuinely runs more than 128 commands, split it.
