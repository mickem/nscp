---
title: "Permission policy reloads are applied in one step and keep the previous policy on failure"
fixed_in: next
severity: "Low"
modules: [core]
action: none
---
A settings reload rebuilt the live permission table in place: it cleared the
rules, re-applied the four switches and then re-added one rule per policy,
each step under its own lock. A check that arrived between the clear and the
last add was evaluated against an *enabled* policy with an empty or partial
table and denied. With checks flowing, every reload produced a burst of
`permissions: denied` UNKNOWN results, and a scheduled check in that window was
tagged `nscp.query_denied` instead of being submitted. Fail-closed, so never a
bypass.

The reload now builds the new policy aside and swaps it in whole, so a check
sees either the old policy or the new one. That also changes what a reload
that cannot read its settings leaves behind: previously the table was cleared
before the first read, so a failure part-way through left enforcement on with
whatever rules had been added so far, or - when the policy had been disabled -
off with no rules. Now the previous policy stays in force unchanged, and the
failure is reported as `permissions: failed to load: …` in the log. A
reload that was meant to turn enforcement on, or to withdraw a grant, and
fails therefore leaves the earlier, more permissive policy running until the
error is fixed and the reload repeated.

**What to do:** nothing required. If you rely on a reload to tighten the
policy, check the log for `permissions: failed to load` after it, or confirm
the new rule count in the `permissions: loaded N rule(s)` debug line.
