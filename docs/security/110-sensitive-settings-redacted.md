---
title: "Settings values for sensitive keys are redacted on read"
fixed_in: 0.16.2
severity: "Medium"
reported_by: "[yagust](https://github.com/yagust)"
modules: [core, WEBServer]
action: conditional
---
The settings read paths — `GET /api/v2/settings/...`,
`GET /api/v2/settings/descriptions/...`, and the `nscp settings --list` /
`--show` CLI — returned values for keys registered sensitive (via
`add_password` / `is_sensitive_key`) in clear text, while the settings `diff`
endpoint already masked them. They now return `***` for sensitive keys, to
match `diff`; internal reads a module makes of its own configuration are
unaffected. This is a defense-in-depth / consistency change, not an
authorization boundary: the values remain stored in plaintext in
`nsclient.ini` (shared with the legacy NRPE/NSCA/NSClient protocols) and are
readable by any principal with write or execute privilege regardless.

**What to do:** nothing required. Tooling that read a secret back out of
`GET /api/v2/settings/...` now receives `***` for keys registered sensitive.
