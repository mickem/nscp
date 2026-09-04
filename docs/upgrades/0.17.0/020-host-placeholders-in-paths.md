---
icon: "🔒"
modules: [core]
---
**`${host}` and friends now resolve in attachment paths and in
`[/includes]`.** Host name placeholders were only expanded in settings urls
and in the url an attachment is fetched from, not in the path it is written
to nor in an included file name. An unknown `${...}` token in a path is not
an error - it resolves to the installation directory - so a configuration
such as `[/attachments] ${shared-path}/${host}.ini = ...` did not fail, it
quietly wrote one file with the installation directory in its name. Those
paths now name the host, which changes where such a file lands: check any
`${host}`, `${hostname}` or `${domain}` you already have under
`[/attachments]` or `[/includes]`, and remove the workaround if you scripted
around this. Configurations without a host name placeholder are unaffected.
When the substitution lands in a local path, the value is sanitized to the
characters a legal host name can contain (see the
[security notice](../security/notices.md#host-name-placeholders-are-sanitized-before-they-land-in-a-local-path)).
Like `nscp settings --switch`, `nscp settings --migrate-to` (and the REST
migrate) now keeps a placeholder you pass it as-is in `boot.ini` while
migrating into the expanded per-host file, so the template survives on a
fleet-managed machine.
