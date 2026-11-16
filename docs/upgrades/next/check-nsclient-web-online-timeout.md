---
icon: "⏱️"
modules: [CheckNet]
action: conditional
---
**`check_nsclient_web_online` now honours `timeout=`.**
The option was documented but never parsed, and the HTTP client has no
deadline of its own, so a remote agent that accepted the connection and never
answered - or anything else listening on that port, which by default is asked
for a TLS handshake it never completes - held the check, and the thread
running it, indefinitely. The timeout is in milliseconds, defaults to 30000,
and bounds the connect, the TLS handshake and every read and write; it is
applied in whole seconds, rounded up. A value that is not a whole number of
milliseconds between 1 and 86400000 is now rejected with UNKNOWN. Nothing to
do unless a check passes an invalid `timeout=` (it used to be ignored), or a
slow agent needs longer than 30 seconds.
