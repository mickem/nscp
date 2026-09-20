---
icon: "🔒 💥"
modules: [packaging]
action: conditional
---
**The Linux systemd unit is sandboxed, the DEB no longer depends on `sudo`, and
`CauseCrashes` is not in any package.** The unit now sets `PrivateTmp=yes`,
`ProtectSystem=full` with `ReadWritePaths` for the state and log directories,
`ProtectHome=read-only`, `UMask=0027` and the kernel-protection directives.
Reading the filesystem is unaffected, which is most of what the checks do. What
changes is that the service and the scripts it runs get a `/tmp` of their own,
and cannot write to `/usr`, `/boot`, `/efi`, `/etc` (`ProtectSystem=full`) or
`/home` and `/root` (`ProtectHome=read-only`). Everywhere else — `/mnt`,
`/srv`, `/media`, `/opt`, `/var` — stays writable, so `check_disk_write` and
any script that writes to a data mount keep working.

If a script or a `check_disk_write` probe of yours writes to one of the
read-only paths, name it in a drop-in rather than editing the unit, which an
upgrade replaces:

```
systemctl edit nsclient
```

```
[Service]
ReadWritePaths=/home/monitoring
```

To go the other way and make everything read-only except what is named, add
`ProtectSystem=strict` in the same drop-in — the unit's `ReadWritePaths` still
applies, so only the extra paths need listing. Check first that nothing the
agent runs writes outside them: a `check_disk_write` probe on a mount you have
not named reports CRITICAL with `Read-only file system`, which is
indistinguishable from a real disk fault.

`NoNewPrivileges` is deliberately **not** set, and the unit says why: the Unix
script launcher tells operators to sandbox a script with `sudo -n -u <account>`,
which needs setuid. Turn it on with a drop-in if nothing on the host escalates.

The DEB drops its `sudo` dependency — the agent never calls it, and it was there
for a documentation example. If an external script escalates with `sudo`, make
sure the package stays installed. `CauseCrashes`, whose only command
deliberately crashes the daemon, is now a diagnostic module behind
`-DBUILD_TESTING_MODULES=ON` and is no longer shipped.
