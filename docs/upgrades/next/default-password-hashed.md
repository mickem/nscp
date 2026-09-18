---
icon: "🔒"
modules: [WEBServer, NSClientServer, NSCAServer]
action: conditional
---
**The shared `/settings/default/password` is stored hashed when `nscp web
install` or `nscp web password --set` writes it.** Nothing to do on a default
install: the WEB admin seed and the check_nt server verify a password against
either form, and a clear-text value you wrote by hand keeps working until you
re-set it (`nscp web password --set <the same value>` hashes an existing
password in place). Three things change:

* `nscp web password --display` cannot show a hashed password. The clear text
  is printed once, by the command that set it; if it is lost, set a new one.
  `--set` now also updates the `admin` user's row, which is what the web login
  checks once the server has booted; `--only-web` changes that row alone.
* **NSCAServer** derives its encryption key from the password string, so a
  hashed value is not a usable key. An agent that serves NSCA from the shared
  default now refuses to load the module, with a log line saying why. Give
  NSCA a clear-text password of its own instead:

```ini
[/settings/NSCA/server]
password = <the NSCA key>
```

* Tooling that read the shared password back out of `nsclient.ini` (or the
  Windows MSI's configuration dialog on an upgrade, which pre-fills the field
  from the file) sees the `pbkdf2-sha256$…` string. Leave it unchanged and the
  hash is kept; type a new value and the MSI writes it in clear text, as it
  always has.

See the [security notice](../security/notices.md#the-shared-default-password-is-stored-hashed).
