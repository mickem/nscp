---
icon: "🔧"
modules: [core]
action: none
---
**The console log is no longer held back in a buffer.** Nothing to do. The
console log backend installs a 64 KB buffer on standard output and nothing
emptied it, so on Windows log output sat there until something else happened
to flush the stream — in `nscp test` the only thing that did was reading the
next line of input, which is why the log appeared to catch up only when you
pressed a key. Every message is flushed as it is written now.

The same fix means a redirected console log streams instead of accumulating:
`nscp test > log.txt`, a container, or a supervisor capturing standard output
now see each line as it is logged rather than nothing until 64 KB has built up
or the process exits.
