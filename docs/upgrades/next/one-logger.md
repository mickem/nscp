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
installed service units keep working unchanged.
