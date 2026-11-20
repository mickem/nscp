---
title: "A refused regular-expression match no longer reads as a clean result"
fixed_in: next
severity: "Low: a check could report OK on data it never matched"
modules: [filters, CheckLogFile, CheckEventLog]
action: none
---
The regular-expression limits introduced in 0.24.0 (a per-check time budget
and a 1 MiB cap on the subject, see
[the resource-limits notice](#resource-limits-on-command-nesting-regular-expression-matching-and-log-file-reads))
were documented to end the check UNKNOWN "rather than quietly
under-matching". That held only when no record matched at all. A match
refused while a record was evaluated was logged to the debug log and read as
*no match*, so:

- in `warning=` or `critical=`, the record was not flagged and the check could
  come back OK - for `text regexp '…'` and `text not regexp '…'` alike;
- in `filter=`, the record was dropped, and a check whose empty state is OK
  (`check_eventlog`, for one) reported "No entries found".

An attacker who can put a large or adversarial string into the data a check
reads (an event message, a log line) could use this to keep a matching
record from raising an alert.

Every error raised while the records are evaluated now ends the check
UNKNOWN, naming the error, through the shared check framework: in `filter`,
`warning`, `critical` and in summary-only thresholds, for every active check.
Real-time filters report it too: one that submits a status submits UNKNOWN,
and one that feeds an event channel, which has no status to carry it, logs it
as an error and counts it on the filter. The same change covers the other ways
an expression used to fail silently; see the
[upgrade note](../setup/upgrading.md).

The failure message names the error and the expression as written, capped to
one short line. It does not quote the record: the subject a match was refused
for can be a megabyte of log text, and echoing it would have overflowed an
NRPE payload and published data the operator never asked to see.

**What to do:** nothing. A check that now reports
`Filter evaluation failed: Refusing to match a regular expression …` or
`… gave up after …` was not evaluating the records it named before either:
anchor or narrow the expression as the message suggests.
