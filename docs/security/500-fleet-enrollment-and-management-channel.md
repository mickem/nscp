---
title: "Fleet: plaintext management urls refused, and enrollment no longer follows planted symlinks"
fixed_in: 0.23.0
severity: "Medium"
modules: [core]
action: conditional
---
Two findings in the enrollment and fleet-sync path, both of which matter on a
packaged Linux install, where `nscp enroll` is documented to run under `sudo`
while the service runs as an unprivileged account.

#### A management url that is not https

The enrollment response names `mtls_url`, the base for every later call: the
desired-state poll, the bundle download, the certificate renewal. That channel
delivers configuration and signed bundles, which is remote code execution by
design, and it is supposed to be protected by the agent's client certificate
and by the server certificate pinned at enrollment.

Neither of those does anything on a plain socket. The fail-closed guard that
refuses mTLS without server authentication only existed on the TLS path, so a
response carrying `http://…` — or a url with no scheme at all, which the http
client also opens on a plain socket — built a plain TCP client, ignored the
certificate and the pin, and ran the whole management channel unauthenticated,
with nothing in the log to say so.

Now: enrollment refuses a `mtls_url` that is not `https://`, the sync loop
refuses to start on a stored one, and the http client refuses outright to
attach a client certificate or a pinned CA to a non-TLS transport. Where
plaintext is genuinely wanted, `nscp enroll --insecure` records the decision in
the manifest — the same flag that already allows a plaintext enrollment url —
and `[tls] allow plaintext = true` in `boot.ini` allows it for a manifest that
predates the field. Either way the sync logs `INSECURE` on every start, naming
what is no longer being checked.

#### Enrollment writes followed symlinks

`adopt_owner` was written carefully — `O_NOFOLLOW`, `openat` at every level —
because the directory it walks belongs to the untrusted service account. The
two writes that happen before it were not. The enrollment manifest was written
by creating `agent-state.json.tmp` with a plain `open(O_CREAT|O_TRUNC)`, and
the `fleet.ini` placeholder with a plain `ofstream`, both in a directory that
account owns.

A compromised service account could pre-create either name as a symlink to any
file on the system; the next `sudo nscp enroll` would then truncate the target
as root. `fleet/fleet.ini -> /etc/nologin` is the cheap version,
`agent-state.json.tmp -> /etc/shadow` the expensive one.

Both writes now open the containing directory `O_NOFOLLOW|O_DIRECTORY` and
create the file through that descriptor with `O_CREAT|O_EXCL|O_NOFOLLOW`. A
stale temporary from a crashed run is unlinked first, which removes a planted
link without touching whatever it points at; losing the race between the unlink
and the create costs a failed enrollment, not a followed link. Replacing an
existing manifest still works, because the rename replaces the name rather than
writing through the inode behind it. Windows is unaffected: the installer and
the service both run as SYSTEM.

**What to do:** nothing on a host enrolled against an `https://` fleet server.
If a host was enrolled against a plaintext url, it stops syncing after the
upgrade and says so in the log; re-enroll against https, or re-enroll with
`--insecure` if that is really what you want.
