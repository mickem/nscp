---
icon: "🔒"
modules: [SMTPClient]
---
**SMTP client security hardening.** The same pass closed a set of
trust gaps in `SMTPClient`: data pipelined into the STARTTLS greeting is
refused rather than trusted as post-handshake input, the EHLO name is
validated for command injection before it reaches the wire, and ESMTP
capabilities are matched per reply line instead of by substring search. No
configuration change is needed. See
[Security notices](../security/notices.md#smtp-client-security-hardening).
