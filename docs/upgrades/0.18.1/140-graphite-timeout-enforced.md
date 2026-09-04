---
icon: "⏱️"
modules: [GraphiteClient]
action: conditional
---
**The Graphite `timeout` is now enforced — as a budget for the whole
submission.** It was doubly dead before: the value the operator configured
never reached the connection (the lookup read a map the well-known `timeout`
key is not stored in, so the default 30 always won), and the connection did
not use even that — resolution, connect, the TLS handshake and every write
ran with no deadline, so a stalled carbon endpoint could hold the submitting
thread for the OS-level TCP timeout, or indefinitely on a stuck write. The
configured `timeout` (default 30s) is read now and bounds the whole
submission as one budget, like the SMTP client's. A target whose submissions
previously completed by quietly taking longer will now fail at the
configured value; raise `timeout` on targets talking to a slow relay. See
[Security notices](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
