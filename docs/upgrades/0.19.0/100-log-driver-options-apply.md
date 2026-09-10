---
icon: "🔧"
modules: [core]
action: none
---
**`--no-stderr` and the oneline log format now take effect.** Nothing to do
unless you were working around the old behaviour. Both were passed to the log
*level* parser, which does not know them, so they were rejected with
`Invalid log level: no-std-err` in the log and never applied. They reach the
log driver now.
