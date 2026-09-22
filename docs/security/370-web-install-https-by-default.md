---
title: "`nscp web install` serves HTTPS by default"
fixed_in: next
severity: "Low"
modules: [WEBServer]
action: none
---
`nscp web install`, the command the documentation tells operators to run to
turn the web server on, enabled HTTPS only when passed `--https`. The switch
behind it was stored as *false* whenever the flag was absent, so a plain
`nscp web install` wrote an empty `certificate`, generated none, and on older
releases left the REST API and web UI — session keys and passwords included —
on cleartext HTTP. On current releases, where the server refuses to start in
cleartext unless `allow insecure = true` is set, the same install produced a
web server that did not start at all.

The command now sets up HTTPS unless asked for cleartext with `--insecure`:
it generates the self-signed `${certificate-path}/certificate.pem` when it is
missing (private key and certificate in the one file, readable only by the
account running the agent), repairs an earlier install that left the
certificate blank, and writes `allow insecure = false` so a stale opt-in from
an `--insecure` run cannot turn a lost certificate into a cleartext listener.
`--insecure` is the explicit opt-in: it writes `allow insecure = true`, no
certificate, and moves a default port from `8443` to `8080`, and says so.

**What to do:** nothing required. If you ran `nscp web install` on a release
that still fell back to HTTP, re-run it to switch the web server to HTTPS, or
replace the generated self-signed certificate with one of your own through
`--certificate` / `--certificate-key`. See the
[upgrade note](../setup/upgrading.md).
