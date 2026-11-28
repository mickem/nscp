---
icon: "📁"
modules: [CheckSecurity]
action: conditional
---
**`check_file_security` works on Unix, and gained group, mode, metadata and directory-walk support.** The check no longer answers "not supported on this platform" off Windows: it reads the owner, the group, the mode bits and — on Linux — the POSIX ACL of each path, and `service=` resolves the program a systemd unit starts. The rules are the ones Windows already had: a write bit on `other` is **world writable** (CRITICAL), any other user or group with write access is unexpected (WARNING) unless it is `root`, the `root` group, an owner named with `expected-owner` or a trustee named with `allow-write` (`www-data`, `user:…`, `group:…`, `uid:N`, `gid:N`, `other`). New on every platform:

| Addition | What it is |
|---|---|
| `expected-group` option, `group` / `group_sid` / `group_expected` keywords | The owning group (Unix) or the descriptor's primary group (Windows), with an allow-list like `expected-owner`. |
| `mode` keyword | Unix permission bits as four octal digits, e.g. `critical=mode != '0600'`. |
| `sddl` keyword | The raw Windows security descriptor, access masks included. |
| `setuid`, `setgid`, `sticky`, `has_acl` keywords | The Unix special bits, and whether a POSIX ACL is present. |
| `size`, `written`, `access`, `creation`, `age` keywords | File metadata next to the permissions. |
| `max-depth` option | Also inspect what is inside a directory, that many levels down. |

Two things to check if you already run it: the default critical expression now also contains `group_expected = 0` (a no-op until you pass `expected-group`, but a hand-written `critical=` that copied the old default does not pick it up), and a Linux agent that carried the check in its configuration and relied on the UNKNOWN "not supported" answer now gets a real verdict for those paths.
