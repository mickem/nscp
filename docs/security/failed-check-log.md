---
title: "The failed-check log exposes recent check output to failures.list"
fixed_in: 0.27.0
severity: "Low"
modules: [core, WEBServer]
action: conditional
---
The core now keeps the last 50 checks that did not come back OK, and the WEB
server serves them at `/api/v2/failures` and on the web UI's **Failed checks**
page. This is a new place where check output can be read: an entry holds the
check's output lines and performance data, who asked for it (the dispatching
module and, for REST, the user), and the arguments it was called with as far
as `record arguments` keeps them (by default only the allow-listed options
described below).

What a caller with the new `failures.list` grant can therefore see is the
output of checks that *other* callers ran, which the per-query endpoints never
showed. The grant is separate from `queries.execute` and is held by the
built-in `full` role alone; no other built-in role, and no existing custom
role, gets it without an operator adding it. The log is memory-only (nothing
is written to disk), capped (entries, lines, line length, perf values,
arguments), and its `data` blob is dropped.

Two safeguards limit what a secret can leak through it:

* **Only allow-listed arguments are kept by default.** `record arguments`
  takes `none`, `safe` or `all`. Under `safe`, the default, an argument is
  kept only when its option name is on an allow-list of options that
  describe the check - thresholds (`warning`, `critical`), `filter`, the
  syntax options and the subject checked (`time`, `type`, `drive`,
  `service`, `process`, `file`, `path`, `counter`, ...) - so a host, a URL, a
  command line, a query, a user name or a password is dropped whatever it is
  called. That includes `computer`, and an allow-listed option whose value
  starts with `\\`: a UNC path or a remote counter names a host too. Under `all` every argument is kept and the value of any argument
  whose name contains `password` or `token` is replaced by `***` - the same
  rule the client-side command logging applies.
* **A secret echoed in the message** - a check that rejects its options
  repeats them, as in `unrecognised option 'password=hunter2'` - is redacted
  the same way, whether or not arguments are recorded.

These are defense in depth, not an authorization boundary: a check that prints
a secret under another name prints it into the log too, exactly as it does
into the check result the caller receives.

**What to do:** nothing on a default install. Treat `failures.list` as
"may read the output of every check this agent runs" when granting it, and
leave `record arguments` at `safe` (or set it to `none`) unless you need every
argument to debug a failing check: a secret passed under a name that does not
mention a password or a token is kept as it was under `all`. A value on the
allow-list is kept as typed, so a filter that names a secret
(`filter=name = 'hunter2'`) keeps it under `safe` too. Set `enabled = false` under `[/settings/core/failed checks]` to switch
the log off entirely.
