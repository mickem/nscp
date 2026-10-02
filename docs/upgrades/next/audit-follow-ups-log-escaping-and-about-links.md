---
icon: "🔒"
modules: [NRPEServer, NSClientServer, WEBServer]
action: none
---
**NRPE escapes the command name in its log lines, check_nt reports a missing
password once, and the About page only links to `http(s)` homepages.** Three
small hardening fixes from the source review; see the
[security notice](../security/notices.md#audit-follow-ups-nrpe-log-escaping-check_nt-log-flooding-and-about-page-link-schemes).
Nothing to do: a control character in an NRPE command name now shows up in
the log as `\xNN` instead of a line break, the "no password configured"
error for check_nt is logged when the module loads instead of on every
request, and a bundled web dependency whose homepage is not an `http(s)` URL
is linked to its npmjs.com page instead.
