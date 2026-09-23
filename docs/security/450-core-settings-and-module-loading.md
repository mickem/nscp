---
title: "Core: module names as paths, remote settings migration, sensitive-key names, service sandboxing"
fixed_in: next
severity: "Low"
modules: [core, packaging, NRDPClient, WEBServer]
action: conditional
---
Six findings from a review of the core service, the settings engine and the
Linux packaging.

#### A module name was used as a raw filesystem path

The right-hand side of a `[/modules]` entry, and the name in a settings
`Control.LOAD` request, reached the loader as-is. An absolute value replaced the
module path outright and `..` walked out of it, so `/tmp/evil.so = enabled` or
`C:\Users\Public\evil.dll = enabled` loaded an arbitrary shared object into the
`SYSTEM` or `root` process. Both writers are already code-execution-equivalent —
a `settings.put` grant plus a reload, or a script plugin issuing a registry
query — but naming a module should not also mean naming a file anywhere on the
host. The loader now requires a single path segment, the same rule the REST
module routes have applied for a while.

#### The module fallback searched the current directory

When a module was not found in the module path, the loader fell back to
`./modules` — relative to the process's working directory. An administrator
running `nscp client` or `nscp test` from a user-writable folder therefore let
whoever could write there supply the library. The fallback resolves against
`${exe-path}` now, which is what it was always meant to name.

#### Windows module dependencies still resolve through the legacy search order

Modules are loaded with `LoadLibrary`, which resolves their *imports* through
the legacy search order, current directory and `PATH` included. Narrowing that
to the default search directories was attempted and reverted: those flags take
only a fully qualified path, and confining the search also stops a module
finding the shared libraries it links against, which the agent's own plugins
do. It needs its own change, made and verified on Windows, rather than riding
along with this one.

What is fixed here is the path the loader is *given* — the two items above —
which is where a caller had influence. Resolving a module's imports is a
hardening item with no caller-controlled input, and it is tracked separately.

#### A settings `Control.LOAD` could name an http:// store

`Control.LOAD` and `Control.SAVE` migrate between settings stores, and the store
factory honours every protocol it knows — including `http` and `https`. A caller
able to issue a settings control could therefore make the agent pull its whole
configuration from a host of their choosing, or push the local configuration,
credentials included, to one. Migration is now refused for a remote context; a
remote settings source remains a deliberate boot.ini decision, where
[notice 280](notices.md#remote-settings-sources-must-be-https) already requires
https.

#### Redaction only covered modules that were loaded

`is_sensitive_key` was an exact lookup against what the loaded modules had
registered. A password left in `nsclient.ini` for a module that is disabled or
not installed on this host — an NRPE client target password, a WEB password with
`WEBServer` off, an NRDP token — was returned in clear by
`GET /api/v2/settings` to any `settings.get` role, and by `nscp settings --list`
without `--load-all`. The core now also matches on the key's name
(`password`, `passwd`, `passphrase`, `token`, `secret`, `api key`, `credential`,
and a bare `key` on a target), so a credential is masked whether or not its
module is running. Separately, the NRDP `key` alias — one of three spellings of
the same token, and the only one not registered with `add_password` — is
registered as a password.

#### The Linux service ran without any hardening

The unit correctly ran as an unprivileged account and had nothing else: no
`UMask`, none of the kernel-protection directives. Given the agent executes
operator-defined and fleet-delivered scripts by design, that is a larger blast
radius than it needs. The unit now sets `UMask=0027`,
`ProtectKernelTunables=yes`, `ProtectKernelModules=yes`,
`ProtectControlGroups=yes`, `RestrictSUIDSGID=yes`, `RestrictRealtime=yes` and
`LockPersonality=yes`.

`UMask=0027` is what stops `nsclient.db`, `fleet.ini`, `applied-state.json` and
the unsealed contents of a bundle landing world-readable: all four are written
through a plain C++ `ofstream`, which asks for 0666 and leaves the rest to the
umask. The log file is not among them — the logger creates that with an explicit
0640 of its own.

`ProtectSystem`, `ProtectHome` and `PrivateTmp` are deliberately **not** set.
They are the three directives that give the service a mount namespace, and
systemd implements each as bind mounts that appear in the service's own
`/proc/mounts`. For a monitoring agent the mount table is reported data, so
they do not merely restrict the process, they falsify its output:
`check_drivesize` enumerates `/proc/mounts` skipping only pseudo filesystems
and repeated mount points, so a `ProtectSystem` bind — the root filesystem's
own type at a fresh mount point — becomes an extra drive reporting `/`'s usage
with `writable = 0`, and the disk-free collector applies the identical filter,
so the phantom rows reach perfdata and OpenMetrics. `check_mount` can no longer
distinguish a filesystem that genuinely went read-only, and `check_disk_write`
reports the very fault it exists to detect. Reporting the kernel's real view is
worth more than sandboxing an already-unprivileged process into reporting a
different one; operators who want them can add them with a drop-in.
`NoNewPrivileges` is left off for a separate reason, documented in the unit: the
Unix script launcher tells operators to sandbox a script with
`sudo -n -u <account>`, which needs setuid.

`CauseCrashes`, whose `crash_client` command dereferences a null pointer on
purpose, is no longer built into packages — it is a diagnostic module behind
`-DBUILD_TESTING_MODULES=ON`. The DEB no longer depends on `sudo`, which the
agent itself never calls.

**What to do:** nothing on a default install. A `[/modules]` entry naming a path
rather than a module file name will now be refused with an error in the log; put
the module in the module path and name it. If you drive `settings --load` /
`--save` against an `http://` context, migrate locally instead. If an external
script escalates with `sudo`, keep `sudo` installed — the package no longer
pulls it in.
