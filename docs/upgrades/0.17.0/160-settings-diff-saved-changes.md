---
icon: "🔧"
modules: [core, WEBServer]
---
**The settings diff no longer reports changes that were already saved.**
`get_changes()` — behind the REST settings `diff` endpoint and any
operator-facing "what am I about to save?" view — kept listing an edit for
the lifetime of the process after it had been written, reporting it as a
`modified` entry whose old value equalled its new one. Tooling that treated
a non-empty diff as "unsaved work pending" no longer needs to special-case
that; no configuration change is needed.
