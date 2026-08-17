# File layout

Where NSClient++ keeps its files, and which account owns them. This matters most
on Linux, where the agent runs as an unprivileged service account and cannot
write into its own installation directory.

The paths below are the defaults; every one of them is a
[path variable](settings.md#paths) that can be redirected from `boot.ini`.

## Linux

A packaged install (DEB/RPM) splits into three trees: the package itself, the
configuration, and the writable state.

```
/usr/lib/nsclient/            root:root     ${shared-path} - the package, never written at runtime
├── modules/                                ${module-path}
├── web/                                    ${web-path}
├── scripts/                                ${scripts}
└── security/                               ${certificate-path}
    ├── certificate.pem                     server TLS certificate (WEB, NRPE, NSCA, NSClient)
    └── nrpe_dh_*.pem                       shipped DH parameters

/etc/nsclient/                root:root     configuration
├── nsclient.ini                            your configuration, plus the fleet include
└── boot.ini                                optional; [paths] overrides live here

/var/lib/nsclient/            nsclient      ${data-path} - writable state
├── security/
│   └── agent-state.json      0600          fleet identity: keys, certificates, server urls
├── fleet/                                  ${fleet-folder} - owned by the fleet sync
│   ├── fleet.ini                           rendered fleet configuration (included by nsclient.ini)
│   ├── applied-state.json                  what is currently applied
│   ├── cache/                              downloaded bundles
│   └── scripts/                            scripts staged from bundles
└── nsclient.db                             agent storage (event log bookmarks, ...)

/var/log/nsclient/            nsclient      ${log-path}
└── nsclient.log                            created by the service on first write
```

The service runs as `nsclient` (see `User=` in the systemd unit), while
`nscp enroll` is normally run with `sudo`. Enrollment therefore hands the files
it writes to the account that owns `${data-path}`, so the service can read its
own identity.

<!-- @formatter:off -->
!!! note "Why writable state is not in the install directory"
    `/usr` has to be mountable read-only, and the package directory is owned by
    root while the service is not. An agent that wrote its identity there
    enrolled successfully and then never appeared in the fleet, because the sync
    could not read the file back. Anything the agent writes therefore lives under
    `/var/lib/nsclient` or `/var/log/nsclient`, which the package creates and
    hands to the service account.
<!-- @formatter:on -->

## Windows

Windows has two layouts. **Legacy** is the default and keeps everything beside
the executable; **modern** moves the writable state to `%ProgramData%` and locks
it down. Pick one per installation in `boot.ini`:

```ini
[layout]
mode = modern
```

<!-- @formatter:off -->
!!! warning "Modern is experimental — but more secure"
    **What it fixes.** On the legacy layout everything lives in
    `C:\Program Files\NSClient++\`, which any logged-in user can read — including
    `nsclient.ini` (web and NRPE passwords, and any credentials your checks use),
    the server's TLS private key, the fleet identity's private key, and the log.
    The modern layout moves those to `%ProgramData%\NSClient++\` restricted to
    `SYSTEM` and `Administrators`, and stops the agent needing write access to
    its own install directory.

    **Why experimental.** The layout is sound and the migration is tested, but
    upgrades have not been exercised at scale. Prefer it for **new installs**;
    for **existing installs, test the upgrade in your own environment first** —
    configuration management, backups and scripts that reference the old paths
    are what will notice. Nothing moves unless you ask: an upgrade without the
    property keeps the layout the host already has.

    See [Securing NSClient++](../setup/securing.md#file-layout-windows).
<!-- @formatter:on -->

`legacy` (or leaving the section out entirely) keeps the existing behaviour, so
an upgrade changes nothing until you ask it to. An unrecognised mode is treated
as `legacy` and logged as a warning — a typo never half-moves an installation.

### Switching an existing installation

Editing `boot.ini` by hand changes where the agent *looks*, not where the files
*are*. Use the migration command instead — it moves the files, then records the
choice:

```shell
nscp settings --migrate-layout modern --dry-run    # show the plan, change nothing
nscp settings --migrate-layout modern              # do it, then restart the service
```

Run it from an elevated prompt: it writes to the install directory and to a
folder that ends up restricted to SYSTEM and administrators. It is safe to
re-run — a file already at the destination is left alone, so an interrupted
migration can simply be repeated.

What moves, and what does not:

| | |
|---|---|
| `nsclient.ini`, `security\*`, `fleet\`, `cache\`, `log\`, `crash-dumps\` | moved — per-machine state |
| `nrpe_dh_*.pem` | stays — shipped with the package; NRPE finds them through [`${nrpe-dh}`](#nrpe-dh) |
| `windows-ca.pem` | dropped — re-exported from the Windows ROOT store at every start, and a stale trust bundle is worse than none |
| `boot.ini`, `modules\`, `web\`, `scripts\`, the executables | stay — program content. `boot.ini` is what points at the shared folder, so it cannot live inside it; `web\` is [deliberately not writable by the service](#the-web-root-stays-with-the-program) |

Anything else you put under `security\` — your own CA bundle, an extra
certificate — moves with the rest, because stranding a trust anchor breaks TLS
quietly.

`--migrate-layout legacy` moves everything back the same way.

### Choosing the layout at install time

The MSI takes a `LAYOUT` property, on a fresh install and on an upgrade alike:

```shell
msiexec /i NSCP-x64.msi LAYOUT=modern /qn
```

On an upgrade it moves the existing installation's files for you, the same way
the command above does. Leaving the property out keeps whatever the host
already uses — so an upgrade never moves an installation that did not ask to
move, and never moves a modern one back to legacy.

The property is one-way: the installer only moves files *into* the protected
folder, so `LAYOUT=legacy` against a host that is already modern is logged and
ignored rather than recording a layout whose files had not moved. Use
`nscp settings --migrate-layout legacy` for that direction.

<!-- @formatter:off -->
!!! note "Experimental, and opt-in"
    The modern layout has no UI yet: the MSI property and
    `nscp settings --migrate-layout` are the two ways to select it. Recommended
    for new installs; for upgrades, try it on a representative host before
    rolling it out - see [Securing NSClient++](../setup/securing.md#should-you-use-it).
<!-- @formatter:on -->

### Legacy (default)

The service runs as LocalSystem, which can read and write the whole install
directory, so the layout stays together under the installation folder.

```
C:\Program Files\NSClient++\        ${shared-path} = ${exe-path}
├── nscp.exe
├── boot.ini                        [paths] overrides live here
├── nsclient.ini                    your configuration, plus the fleet include
├── nsclient.log                    the default log file sits here, not under log\
├── modules\                        ${module-path}
├── scripts\                        ${scripts}
├── web\                            ${web-path}
├── log\                            ${log-path} (the folder token; set `file name` to use it)
├── cache\                          ${cache-folder}
├── crash-dumps\                    ${crash-folder}
├── security\                       ${certificate-path}
│   ├── agent-state.json            fleet identity: keys, certificates, server urls
│   ├── certificate.pem             server TLS certificate
│   └── nrpe_dh_*.pem
└── fleet\                          ${fleet-folder} - owned by the fleet sync
    ├── fleet.ini
    ├── applied-state.json
    ├── cache\
    └── scripts\
```

### Modern (opt-in)

Only `${shared-path}` moves; everything is defined relative to it, so the whole
writable half follows and the read-only half stays with the program.

```
C:\Program Files\NSClient++\        ${exe-path} - the program, plus boot.ini
├── nscp.exe
├── boot.ini                        [layout] and [paths] live here
├── modules\                        ${module-path}
├── scripts\                        ${scripts}
├── security\
│   └── nrpe_dh_*.pem               shipped DH parameters, ${nrpe-dh}
└── web\                            ${web-path}

C:\ProgramData\NSClient++\          ${shared-path} - SYSTEM + Administrators only
├── nsclient.ini                    your configuration
├── security\                       ${certificate-path}
│   ├── agent-state.json            fleet identity
│   ├── certificate.pem             server TLS certificate
│   └── windows-ca.pem              ${ca-path}, re-exported at every start
├── fleet\                          ${fleet-folder}
├── log\                            ${log-path}
├── cache\                          ${cache-folder}
└── crash-dumps\                    ${crash-folder}
```

The agent creates that folder with an explicit DACL granting **SYSTEM** and
**Administrators** full control, and breaks inheritance so `%ProgramData%`'s
default `Users: Read & Execute` does not apply. Without that, moving the
configuration (which holds passwords) and the fleet private key out of Program
Files would make them readable by every account on the machine — a downgrade,
not a fix.

That has a consequence worth knowing: **an ordinary user cannot read the
configuration on the modern layout**, so administrative commands need an
elevated prompt. Running checks does not — the CLI logs to the console and
needs no access to the machine's files.

`check_nrpe` and `check_nscp` read the same `[layout]` setting from the
`boot.ini` next to them, so they resolve `${certificate-path}` to the same place
the service does.

### The web root stays with the program

`${web-path}` does **not** follow the layout. It resolves to `${exe-path}\web`
on Windows and to the package directory on unix — both places the running
service cannot write to.

That is deliberate, and it is a security property rather than an oversight. The
web UI is program content: on Windows it ships in the MSI and is never
downloaded, and on unix `nscp web install` fetches it as root. Nothing about
serving those pages requires the service account to be able to change them, and
a web root the service *can* write to turns any compromise of the service into
browser-executed code delivered from your own agent.

The practical consequence is that installing or updating the web UI needs an
elevated prompt (Windows) or `sudo` (unix), the same as any other change to the
installed program.

### `${nrpe-dh}`

The NRPE Diffie-Hellman parameters are the one piece of `security\` that does
not move: they are shipped with the package, replaced on upgrade, and owned by
the installer. That leaves them in Program Files while `${certificate-path}`
points at `%ProgramData%`, so they get a token of their own.

`${nrpe-dh}` is a lookup rather than a fixed path. It answers with
`${modern-nrpe-dh}` (`${certificate-path}`) when that folder actually contains
`nrpe_dh_*.pem`, and otherwise with `${legacy-nrpe-dh}`
(`${exe-path}\security`), where the installer put them. Practical effect: the
shipped parameters are found on either layout, and dropping your own
`nrpe_dh_2048.pem` in beside the rest of the writable state overrides them.
Both candidates are ordinary path tokens, so `boot.ini`'s `[paths]` section and
`--path-override` can name either — or `nrpe-dh` itself, which skips the lookup.

On unix there is no split: the parameters are installed into the package
directory that `${certificate-path}` already names, and both candidates resolve
there.

## Logging

The **service** writes `${log-path}/nsclient.log`. Every other invocation —
`nscp client`, `nscp test`, `check_nrpe` — logs to the console and writes no
file, so running a command never needs write access to the install directory or
to any machine-wide folder.

Ask for a file explicitly if you want one:

```shell
nscp client --query check_ok --log-backend threaded-file
```

<!-- @formatter:off -->
!!! note "Changed in 0.17"
    On Windows every invocation used to write to a log file beside the
    executable, which an ordinary user could not open — the command worked but
    logging silently degraded. The file backend is now the service's, and the
    default log file moved from `${exe-path}/nsclient.log` into `${log-path}`.
<!-- @formatter:on -->

## Fleet-managed configuration

On both platforms the fleet server's configuration arrives as a separate file
that your configuration includes:

```ini
[/includes]
fleet = ${fleet-folder}/fleet.ini
```

`nscp enroll` writes that include, and so does the MSI when installed with
`FLEET_SERVER`/`FLEET_TOKEN`. The entry is stored unexpanded, so redirecting
`fleet-folder` moves the sync's whole working directory - rendered
configuration, staged scripts and bundle cache - in one step.

Everything under `${fleet-folder}` belongs to the sync and is rewritten on every
change; do not edit those files by hand.

### Local settings win

A setting lookup reads your configuration first and only falls back to an
included file when the key is not set. **Anything configured locally therefore
overrides the fleet-managed value for that key**, and only that key - everything
you have not set locally still comes from the fleet server.

That is a legitimate way to run a host: pin the few settings this machine needs
and manage the rest centrally. It is only a problem when it is a surprise, so
`nscp enroll` points it out if the host already has configuration of its own:

```text
NOTE: this host already has local configuration, and local settings take precedence
      over the fleet-managed ones. ...
```

To hand a setting back to fleet management, remove it from `nsclient.ini` and
let the include supply it again.

Each agent also reports *whether* it has local configuration to the fleet server,
as a `local_config_present` flag on its state report, so the server can show which
hosts are only partly centrally managed. The flag says nothing about **what** is
configured locally - no local configuration is ever uploaded.
