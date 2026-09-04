---
icon: "📨 🔒"
modules: [SyslogClient]
---
**Syslog messages now carry the RFC 3164 HOSTNAME field, and several
syslog options work for the first time** (see the
[security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework)).
What changes on the wire and in behaviour:

- Datagrams now read `<PRI>TIMESTAMP HOSTNAME TAG MESSAGE`. The `hostname`
  setting under `[/settings/syslog/client]` — until now read but never
  used — fills the HOSTNAME field (default `auto`, the machine name).
  Receivers that promoted the tag (default `NSCA`) to origin host will now
  file records under the real host name; adjust any log-parsing rule that
  keyed on the old, hostname-less format.
- The `tag_syntax` and `message_syntax` target settings now reach the
  wire. They were stored under keys the sender never read, so a
  settings-defined target sent an empty tag and dropped the message text.
- The per-state severity options (`ok-severity`, `warning-severity`,
  `critical-severity`, `unknown-severity`) passed as command arguments now
  take effect; they too were stored under keys that were never read.
- An unknown `severity` or `facility` name now degrades to priority `<13>`
  (user.notice) instead of `<0>` — which is kernel.**emergency**, a
  priority many receivers page or broadcast on. A missing per-state
  severity now falls back to the base `severity` instead of tripping that
  fallback.
- All C0 control bytes and DEL in the outgoing line are replaced with
  spaces (previously only CR, LF and NUL), so check output cannot smuggle
  ANSI escape sequences into the receiver's log.
