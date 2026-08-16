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
