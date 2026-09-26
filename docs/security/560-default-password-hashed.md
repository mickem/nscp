---
title: "The shared default password is stored hashed"
fixed_in: next
severity: "Low"
modules: [WEBServer, NSClientServer, NSCAServer]
action: conditional
---
The per-user web passwords under `/settings/WEB/server/users/<name>` are
stored as salted PBKDF2-SHA256 hashes, but the shared `password` under
`/settings/default` — the value `nscp web install` generates, the Windows MSI
writes, and the web admin seed and the check_nt server read — stayed in clear
text in `nsclient.ini`. Anyone who could read the file (the service account
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

`NSCAServer` is different, because NSCA has no password *check* — the string
is the key material the payload is encrypted with, and every client must know
it. A hashed value there is a key nobody has, so the module now refuses to load
on one, with a log line naming the fix, instead of silently rejecting every
submission. The clear-text value goes under `/settings/NSCA/server`, the
section NSCA reads before falling back to the shared default. Nothing rewrites
an existing clear-text value on its own: `nscp web install` re-run without
`--password` leaves the shared value exactly as it found it (it still hashes
the `admin` row, which is its own), so the migration is explicit
(`nscp web password --set`) and an agent that serves NSCA from the shared
default keeps working until its operator chooses to move the key. When a
command *is* about to hash the shared value and an enabled `NSCAServer` would
have taken its key from it — encryption on, no `password` of its own — the
command says so in its output rather than leaving it to the upgrade notes.

Two limits are worth knowing. The Windows MSI still writes the value typed
into its configuration dialog in clear text, since that value is also the one
the operator is told to note down for the first login. And a hash protects
only the password: `nsclient.ini` still holds the client-side passwords,
tokens and keys the agent needs in clear form, so the file permissions remain
the boundary around it.

**What to do:** nothing required. To hash a password that is already on disk,
re-set it with `nscp web password --set <password>` (the same value is fine).
If the agent serves NSCA from the shared default, first give NSCA its own
`password` under `[/settings/NSCA/server]`, or the module will refuse to load
after the hash is written. See the [upgrade note](../setup/upgrading.md).
