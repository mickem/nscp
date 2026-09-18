---
icon: "🔒 💥"
modules: [packaging]
action: conditional
---
**The Linux systemd unit is sandboxed, the DEB no longer depends on `sudo`, and
`CauseCrashes` is not in any package.** The unit now sets `PrivateTmp=yes`,
`ProtectSystem=strict` with `ReadWritePaths` for the state and log directories,
`ProtectHome=read-only`, `UMask=0027` and the kernel-protection directives.
Reading the filesystem is unaffected, which is most of what the checks do; what
changes is that the service and the scripts it runs cannot write outside the
state and log directories, and see a `/tmp` of their own. If a script of yours
writes somewhere else, add a drop-in rather than editing the unit, which an
upgrade replaces:

```
systemctl edit nsclient
```

`NoNewPrivileges` is deliberately **not** set, and the unit says why: the Unix
script launcher tells operators to sandbox a script with `sudo -n -u <account>`,
which needs setuid. Turn it on with a drop-in if nothing on the host escalates.

The DEB drops its `sudo` dependency — the agent never calls it, and it was there
for a documentation example. If an external script escalates with `sudo`, make
sure the package stays installed. `CauseCrashes`, whose only command
deliberately crashes the daemon, is now a diagnostic module behind
`-DBUILD_TESTING_MODULES=ON` and is no longer shipped.
