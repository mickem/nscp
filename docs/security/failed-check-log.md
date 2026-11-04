---
title: "The failed-check log exposes recent check output to failures.list"
fixed_in: next
severity: "Low"
modules: [core, WEBServer]
action: conditional
---
The core now keeps the last 50 checks that did not come back OK, and the WEB
server serves them at `/api/v2/failures` and on the web UI's **Failed checks**
page. This is a new place where check output can be read: an entry holds the
check's output lines and performance data, who asked for it (the dispatching
module and, for REST, the user), and - only when `record arguments` is on -
the arguments it was called with.

What a caller with the new `failures.list` grant can therefore see is the
output of checks that *other* callers ran, which the per-query endpoints never
showed. The grant is separate from `queries.execute` and is held by the
built-in `full` role alone; no other built-in role, and no existing custom
role, gets it without an operator adding it. The log is memory-only (nothing
is written to disk), capped (entries, lines, line length, perf values,
arguments), and its `data` blob is dropped.

Two redactions limit what a secret can leak through it:

* **Arguments are off by default**, and when `record arguments` is on the
  value of any argument whose name contains `password` or `token` is replaced
  by `***` - the same rule the client-side command logging applies.
* **A secret echoed in the message** - a check that rejects its options
  repeats them, as in `unrecognised option 'password=hunter2'` - is redacted
  the same way, whether or not arguments are recorded.

These are defense in depth, not an authorization boundary: a check that prints
a secret under another name prints it into the log too, exactly as it does
into the check result the caller receives.

**What to do:** nothing on a default install. Treat `failures.list` as
"may read the output of every check this agent runs" when granting it, and
leave `record arguments` off unless you need the arguments to debug a failing
check. Set `enabled = false` under `[/settings/core/failed checks]` to switch
the log off entirely.
