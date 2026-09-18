---
title: "Linux packages install nsclient.ini and the log directory owner-readable"
fixed_in: next
severity: "Medium on Linux installs with multiple local accounts"
modules: [packaging]
action: conditional
---
`nsclient.ini` holds the web admin password, the NRPE and NSCA passwords and
the submit-client tokens in plaintext. That is what the file is for — the agent
has to be able to read them — so the boundary around it is the filesystem, which
is what [CVE-2025-34078](#cve-2025-34078-local-privilege-escalation-via-plaintext-credentials) says
in the notice that covers it.

The DEB and RPM packages did not draw that boundary. `/etc/nsclient/nsclient.ini`
was installed `root:root 0644`, so every local account could read the
credentials — and then present them to the agent's own listeners, which run
checks and, where configured, external scripts. `/var/log/nsclient` was
world-readable and world-traversable too, and a debug log echoes check
arguments and settings paths.

The packages now install:

| Path | Owner | Mode |
|------|-------|------|
| `/etc/nsclient` | `root:nsclient` | `0750` |
| `/etc/nsclient/nsclient.ini` | `root:nsclient` | `0640` |
| `/var/log/nsclient` | `nsclient:nsclient` | `0750` |
| `/var/log/nsclient/nsclient.log` | `nsclient:nsclient` | `0640` |

The service account reads the configuration through the group and cannot write
it; nobody else sees either. The post-install scripts re-apply this on upgrade,
so a host installed before this release is fixed by upgrading rather than by
hand. They do so only when the `nsclient` group exists: narrowing the mode
without also handing the group to the service account would lock the daemon out
of its own configuration, so in that unusual case the scripts print a warning
and leave the permissions alone.

Windows is unaffected: the installer already sets a restrictive DACL.

**What to do:** nothing, if nothing but the agent reads those files. If a
local script or a monitoring user reads `nsclient.ini` or the log directory
without being root or in the `nsclient` group, add it to the `nsclient` group
(or give it its own copy of just what it needs) before upgrading — and treat
any credential that was in that file on a multi-user host as having been
readable by every account on it.
