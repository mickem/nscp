---
icon: "📝"
modules: [core]
action: none
---
**The log is written as it happens, and log handlers are fed from one
thread.** Nothing to do. The core used to pick one of three log backends
(`console`, `file`, `threaded-file`); it now has a single logger that writes
the console and the log file directly, so a line is on disk before the call
that logged it returns, and the last lines before a crash are no longer lost
in a queue. Log-handler modules (the web UI's live log, `check_nscp`'s error
count, ElasticClient, scripts) are fed from one background thread in every
mode, so a slow handler holds up no thread that logs - before, in `nscp test`
and `nscp client`, every log call waited for every handler. `--log-backend
file` and `--log-backend threaded-file` both still turn on the log file, so
installed service units keep working unchanged; `file` now behaves exactly
like `threaded-file`, which means it also writes the console and feeds the log
handlers, where the old `file` backend did neither. The file is still opened
and closed for every line, so log shippers and rotation tools never find it
locked. With `max size` set, the cut back to the newest 70% now runs on the
thread that crossed the limit, which waits for it. At most 10 000 lines wait
for the log handlers; when a handler falls that far behind, the oldest lines
are dropped for the handlers only (never for the console or the file), and
`nsclient.fatal` records when that starts and how many were dropped.
