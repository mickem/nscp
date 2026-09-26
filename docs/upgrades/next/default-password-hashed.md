---
icon: "🔒"
modules: [WEBServer, NSClientServer, NSCAServer]
action: conditional
---
**The shared `/settings/default/password` is stored hashed when `nscp web
install` or `nscp web password --set` writes it, and `NSCAServer` no longer
reads that section at all.** Nothing to do on a default install: the WEB admin
seed and the check_nt server verify a password against either form, and a
clear-text value you wrote by hand keeps working until you re-set it
(`nscp web password --set <the same value>` hashes an existing password in
place). **If you serve NSCA and its key came from the shared default, you do
have to move it** — see the second point. Three things change:

* `nscp web password --display` cannot show a hashed password. The clear text
  is printed once, by the command that set it; if it is lost, set a new one.
  `--set` now also updates the `admin` user's row, which is what the web login
  checks once the server has booted; `--only-web` changes that row alone.
* **`NSCAServer` no longer reads `/settings/default/password`.** That section
  is the password inbound protocols (web UI, check_nt, NRPE) verify a caller
  against, and it is now hashed; NSCA does not verify a password, it encrypts
  with it, so it could never use a hash. **If your NSCA server took its key
  from the shared default, move it before upgrading** — otherwise the module
  loads with an empty key and accepts nothing:

```ini
[/settings/NSCA/server]
password = <the NSCA key>
```

  On a host that also *submits* NSCA, put the key on the client target instead
  and the server picks it up from there — one protocol, one shared secret:

```commandline
nscp nsca install --host <nsca-server> --password <the NSCA key> --encryption aes256
```

  That is a new command; it writes `[/settings/NSCA/client/targets/default]`
  and enables `NSCAClient`. The Windows installer takes the same values as
  `NSCA_SERVER`, `NSCA_PORT`, `NSCA_PASSWORD`, `NSCA_ENCRYPTION` and
  `NSCA_HOSTNAME`. Re-running `nscp web install` *without* `--password` now
  leaves an existing clear-text shared value alone, so a certificate rotation
  cannot migrate a credential behind your back.

* Tooling that read the shared password back out of `nsclient.ini` (or the
  Windows MSI's configuration dialog on an upgrade, which pre-fills the field
  from the file) sees the `pbkdf2-sha256$…` string. Leave it unchanged and the
  hash is kept; type a new value and the MSI writes it in clear text, as it
  always has.

See the [security notice](../security/notices.md#the-shared-default-password-is-stored-hashed).
