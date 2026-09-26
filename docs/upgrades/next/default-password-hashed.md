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
place).

Which *direction* of NSCA you run decides whether the second half touches you
at all. Sending passive results from this agent to an NSCA daemon —
`NSCAClient`, which is how NSCA is used in almost every installation — is
**unaffected**: its key has always lived on the client target
(`/settings/NSCA/client/targets/default/password`) and was never read from the
shared section. Only `NSCAServer`, the rarely used listener that lets this
agent *receive* NSCA submissions from somewhere else, ever inherited the shared
password. **If you run that listener and its key came from the shared default,
you do have to move it** — see the second point. Three things change:

* `nscp web password --display` cannot show a hashed password. The clear text
  is printed once, by the command that set it; if it is lost, set a new one.
  `--set` now also updates the `admin` user's row, which is what the web login
  verifies against once the server has booted; `--only-web` changes that row
  alone.
* **`NSCAServer` — the listener, not the client — no longer reads
  `/settings/default/password`.** That section is the password inbound
  protocols (the web UI and check_nt) verify a caller against, and it is now
  hashed; NSCA does not verify a password, it encrypts with it, so it could
  never use a hash. It is not inherited from anywhere else either — not from
  `NSCAClient`, whose key is what this agent submits to a *remote* daemon with.
  **If you run `NSCAServer` and its key came from the shared default, set it
  under its own section before upgrading** — otherwise the module refuses to
  start, logging `Refusing to start NSCA server: encryption is enabled … but no
  password is set`:

```ini
[/settings/NSCA/server]
password = <the NSCA key>
```

  or, equivalently:

```commandline
nscp nsca install --server --password <the key every submitting host uses>
```

  A host that also *submits* NSCA configures that separately, because the two
  keys are shared with different peers — the listening key with the hosts
  submitting here, the client key with the daemon this agent submits to:

```commandline
nscp nsca install --host <nsca-server> --password <the key from that daemon's nsca.cfg> --encryption aes256
```

  `nsca install` is a new command. Without `--server` it configures submission
  only — it writes `[/settings/NSCA/client/targets/default]` and enables
  `NSCAClient`, never touching the server's section, and warns when an enabled
  `NSCAServer` is still missing a key. With `--server` it configures the
  listener instead: `[/settings/NSCA/server]` and `NSCAServer`. It refuses to
  write a listener with a cipher and no key, since that one cannot start. The Windows installer takes the same values as
  `NSCA_SERVER`, `NSCA_PORT`, `NSCA_PASSWORD`, `NSCA_ENCRYPTION` and
  `NSCA_HOSTNAME`. Re-running `nscp web install` *without* `--password` now
  leaves an existing clear-text shared value alone, so a certificate rotation
  cannot migrate a credential behind your back.

* Tooling that read the shared password back out of `nsclient.ini` (or the
  Windows installer's configuration dialog on an upgrade, which pre-fills the
  field from the file) sees the `pbkdf2-sha256$…` string. The installer hashes
  a password you give it — `NSCLIENT_PWD` on the command line, or the dialog
  field — and leaves a value it only found on disk alone, so leaving the field
  unchanged keeps what is there and typing a new one stores it hashed. A
  password the installer *generates* for itself, when a fresh install is given
  none, also stays in clear text: nobody has seen it, so a hash of it would lock
  the new agent's web UI and check_nt with nothing left to recover. Rotate it
  with `nscp web password --set <password>`. The password no longer reaches the
  MSI log in any of these cases.

See the [security notice](../security/notices.md#the-shared-default-password-is-stored-hashed).
