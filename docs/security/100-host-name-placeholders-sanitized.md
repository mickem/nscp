---
title: "Host name placeholders are sanitized before they land in a local path"
fixed_in: 0.17.0
severity: "Low"
modules: [core]
action: none
---
With host name placeholders now resolving in attachment target paths and in
`[/includes]` (issue [#458](https://github.com/mickem/nscp/issues/458)), the
system host name is substituted into paths the agent reads and writes with its
service privileges (typically root / SYSTEM). The host name is not fully under
the operator's control — DHCP can set it on some systems, as can any local
privileged process — so a hostile value such as `../../etc/cron.d/evil` must
not be able to redirect where an attachment is written or which file an
include opens. Values substituted into a path are therefore reduced to the
characters a legal RFC-952 host name can contain (letters, digits, `.`, `-`,
plus `_`): anything else becomes `_`, and a dots-only value (`.`/`..`) becomes
`_`. A legitimate host name comes through unchanged; settings urls and the
submit clients' host name specs are not affected, since there the value never
names a local file. The same expansion pass no longer aborts settings boot if
`gethostname()` itself fails — the placeholders are then left unresolved.

**What to do:** nothing required. Only a host name that is not a legal host
name resolves differently, and only in attachment targets and `[/includes]`.
