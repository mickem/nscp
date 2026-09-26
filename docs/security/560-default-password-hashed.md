---
title: "The shared default password is stored hashed"
fixed_in: next
severity: "Low"
modules: [WEBServer, NSClientServer, NSCAServer]
action: conditional
---
The per-user web passwords under `/settings/WEB/server/users/<name>` are
stored as salted PBKDF2-SHA256 hashes, but the shared `password` under
`/settings/default` — the value `nscp web install` generates, the Windows
installer writes, and the web admin seed and the check_nt server read — stayed
in clear text in `nsclient.ini`. Anyone who could read the file (the service account
and administrators on a current install; every local user on older layouts,
see [Securing NSClient++](../setup/securing.md#file-layout-windows)) held the
admin password itself, not a hash of it, and could reuse it against the web
UI, check_nt, and anywhere else the operator had reused it.

`nscp web install` and `nscp web password --set` now store the shared value in
the same `pbkdf2-sha256$<iterations>$<salt>$<hash>` form as the per-user rows,
and the two readers that only compare a password verify through the same
helper, so either form is accepted:

* the WEB server, which seeds the `admin` row from the shared value on first
  boot (a hashed value is copied as it is; a clear-text one is hashed);
* `NSClientServer`, which compares the password a check_nt client sends
  against the stored value in constant time in both cases. check_nt has no
  session to amortise a key derivation over, so the server derives once: the
  first request that proves itself against the hash leaves the clear text in
  memory, and every request after it — right password or wrong — is answered by
  a constant-time compare. Without that, a host inside `allowed hosts` could
  spend tens of milliseconds of agent CPU per packet on a listener that has no
  rate limiter.

The hash string itself is not a credential: it does not authenticate against
either.

`NSCAServer` used to read the same section, and it is the one reader that
cannot: NSCA never *verifies* a password. The string is the key material the
payload is encrypted with, and every submitting client has to know it, so a
hash there is a key nobody has. To be clear about which half of NSCA that is:
`NSCAClient`, the submitting side and the way NSCA is used in almost every
installation, is unaffected — its key has always been on the client target and
was never read from the shared section. `NSCAServer` is the rarely used
listener that lets the agent *receive* NSCA submissions, and only it inherited
the shared password. Sharing one value between "what I verify inbound callers
with" and "the key I share with a remote server" was the mistake, and hashing
only made it visible. **`NSCAServer` no longer inherits
`/settings/default/password`.** Its key comes from its own section, or failing
that from the default target of `NSCAClient`
(`/settings/NSCA/client/targets/default/password`) — one protocol, one shared
secret per peer, so an agent that both submits and receives NSCA configures it
once. It still refuses to load on a value that *is* a stored hash, since that
is never a usable key, but reaching that now takes a deliberate paste rather
than an inherited default.

That leaves the shared section holding only what it is for: passwords inbound
protocols verify a caller against. `nscp nsca install --host <server>
--password <key> --encryption <cipher>` configures the submission side in one
command, and the Windows installer takes the same three as `NSCA_SERVER`,
`NSCA_PASSWORD` and `NSCA_ENCRYPTION` (the key hidden, as a live credential
should be).

Nothing rewrites an existing clear-text value on its own either: `nscp web
install` re-run without `--password` leaves the shared value exactly as it
found it (it still hashes the `admin` row, which is its own), so migrating it
is an explicit `nscp web password --set`.

The Windows installer follows the same rule: a password given on its command
line (`NSCLIENT_PWD`) or typed into its configuration dialog is hashed before
it is written, and it never lands in the MSI log. A value it merely found on
disk — which is what pre-fills the dialog on an upgrade — is left exactly as it
is, so an upgrade migrates nothing on its own.

One limit is worth knowing: a hash protects only this password.
`nsclient.ini` still holds the client-side passwords, tokens and keys the agent
needs in clear form — it has to use those, not verify them — so the file
permissions remain the boundary around it.

**What to do:** if the agent runs `NSCAServer` (the listener — not the common
`NSCAClient`, which is unaffected) and that listener relied on the shared
default for its key, put the key where NSCA now reads it —
`[/settings/NSCA/server]`, or the `NSCAClient` default target if this host also
submits — before upgrading, or the module logs an empty-key warning and accepts
nothing. Otherwise nothing is required. To hash a password already on disk, re-set it with `nscp web
password --set <password>` (the same value is fine). See the
[upgrade note](../setup/upgrading.md).
