---
icon: "🔒"
modules: [packaging]
action: conditional
---
**The Linux systemd unit is hardened, the service runs under its own group, the
DEB no longer depends on `sudo`, and `CauseCrashes` is not in any package.** The
unit sets `UMask=0027`, `ProtectKernelTunables=yes`, `ProtectKernelModules=yes`,
`ProtectControlGroups=yes`, `RestrictSUIDSGID=yes`, `RestrictRealtime=yes` and
`LockPersonality=yes`. None of those change anything the agent observes, so no
check answers differently after the upgrade.

**`Group=nsclient` closes a disclosure on Debian and Ubuntu.** Without it the
service ran under the account's *primary* group, and the postinst creates the
account with `adduser --system` and adds `nsclient` only as a supplementary
group — so the primary group stayed `nogroup`. With `UMask=0027` that left every
file the service wrote at runtime readable by `nogroup`, which many unprivileged
daemons share, and `/var/lib/nsclient` is `0755` from the postinst's `mkdir -p`,
so `nsclient.db` and the `fleet/` tree — including the enrollment key — were not
covered by the directory either. Upgrading fixes existing installs; no action is
needed beyond that. If you created the service account by hand, make sure an
`nsclient` group exists, since the unit now names it.

The unit also gains `Documentation=` and `After=network.target`. The ordering
guarantees little at start-up, but at shutdown it stops the agent before the
network goes down, which is what a daemon holding client connections and
submitting results wants.

**`ProtectSystem`, `ProtectHome` and `PrivateTmp` are deliberately not set**,
and the unit explains why at length. They are the three directives that give a
service a mount namespace of its own, and systemd implements each of them as
bind mounts that appear in the service's own `/proc/mounts`. For most daemons
that only restricts what the process may do; for a monitoring agent the mount
table is part of what it reports, so those directives would change its answers —
in exactly the direction the checks exist to detect:

| check | what a mount namespace does to it |
| --- | --- |
| `check_drivesize` (no `drive=`) | `/usr`, `/etc` and `/boot` appear as extra drives, each reporting `/`'s usage, so a threshold on `/` fires several times over |
| `check_drivesize`, keyword `writable` | reads `0` for every one of those rows, and for `/boot/efi` |
| disk-free metrics | the collector applies the same filter, so the phantom rows reach perfdata and `/api/v2/openmetrics` |
| `check_mount`, `options=rw` | reports `missing options: rw, exceeding options: ro`, and a filesystem that genuinely went read-only can no longer be told apart |
| `check_disk_write` | fails with `read-only file system` for any target outside `ReadWritePaths` — the same message as the fault it looks for |
| `check_files`, `check_logfile` over `/tmp` | report on the private tree, and a plugin keeping state there loses it on every restart |

If you accept that for your own setup, add them back with a drop-in rather than
by editing the unit, which an upgrade replaces:

```
systemctl edit nsclient
```

```
[Service]
ProtectSystem=full
```

`NoNewPrivileges` is deliberately **not** set either, and for a different
reason: the Unix script launcher tells operators to sandbox a script with
`sudo -n -u <account>`, which needs setuid. `CapabilityBoundingSet=` and a
restrictive `SystemCallFilter=` break it the same way. Turn any of them on with
a drop-in if nothing on the host escalates.

The unit no longer sets `PIDFile=`, and `ExecStart` no longer passes `--pid`:
with `Type=simple` systemd tracks the main process itself and nothing read the
file.

A note on systemd versions, since an unknown key or an unparsable value is
ignored with a warning rather than failing the unit — so a host too old for one
of these loses it silently. `ProtectKernel*` and `ProtectControlGroups` need
232, `LockPersonality` 235, `RestrictSUIDSGID` 242; `UMask` has always been
there. Every official package target has all of them (Ubuntu 24.04 ships
systemd 255, Rocky 9 ships 252). A source build on RHEL 8 (239) loses
`RestrictSUIDSGID`, and on RHEL 7 (219) only `UMask` survives.

The DEB drops its `sudo` dependency — the agent never calls it, and it was there
for a documentation example. If an external script escalates with `sudo`, make
sure the package stays installed. `CauseCrashes`, whose only command
deliberately crashes the daemon, is now a diagnostic module behind
`-DBUILD_TESTING_MODULES=ON` and is no longer shipped.
