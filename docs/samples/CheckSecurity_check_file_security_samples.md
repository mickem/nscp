**Check that a data directory is not world-writable (Windows)**

The default is critical when the path is missing or unreadable, when its owner is
not on the expected list or when an untrusted group can write it.

```
check_file_security "path=C:\Program Files\NSClient++"
L        cli OK: C:\Program Files\NSClient++: owner NT AUTHORITY\SYSTEM, no unexpected write access
```

```
check_file_security "path=C:\ProgramData\app-data"
L        cli CRITICAL: C:\ProgramData\app-data: world writable by Everyone
```

**Check several paths at once**

```
check_file_security "path=C:\Windows\System32" "path=C:\Windows"
L        cli OK: C:\Windows\System32: owner NT SERVICE\TrustedInstaller, no unexpected write access, C:\Windows: owner NT SERVICE\TrustedInstaller, no unexpected write access
```

The default top syntax lists every path. A top syntax without `${list}` gives a
summary instead — and while everything is fine the built-in OK summary is used:

```
check_file_security "path=C:\Windows\System32" "path=C:\Windows" "top-syntax=${status}: ${problem_count} of ${count} path(s) have a problem"
L        cli OK: all 2 path(s) have the expected owner and no unexpected write access
```

```
check_file_security "path=C:\Windows" "path=C:\ProgramData\app-data" "top-syntax=${status}: ${problem_count} of ${count} path(s) have a problem"
L        cli CRITICAL: 1 of 2 path(s) have a problem
```

**Check the binary a service runs**

The image path is read from the service configuration, so a service whose binary
was replaced or moved is caught as well.

```
check_file_security service=EventLog "top-syntax=${status}: ${list}" "detail-syntax=${service} (${path}): ${state}"
L        cli OK: EventLog (C:\Windows\System32\svchost.exe): owner NT SERVICE\TrustedInstaller, no unexpected write access
```

```
check_file_security service=NoSuchService
L        cli CRITICAL: NoSuchService: Service not found: NoSuchService
```

**Require a specific owner**

```
check_file_security "path=C:\Windows\System32\spoolsv.exe" "expected-owner=NT SERVICE\TrustedInstaller"
L        cli OK: C:\Windows\System32\spoolsv.exe: owner NT SERVICE\TrustedInstaller, no unexpected write access
```

```
check_file_security "path=C:\Windows\System32\spoolsv.exe" "expected-owner=NT AUTHORITY\SYSTEM"
L        cli CRITICAL: C:\Windows\System32\spoolsv.exe: unexpected owner NT SERVICE\TrustedInstaller
```

**Allow a trustee that is supposed to have write access**

Allow-list entries are matched by SID, by `DOMAIN\name` or by the bare name.
SIDs are the safe choice on a localized Windows, where `Everyone` and
`BUILTIN\Users` carry translated names.

```
check_file_security "path=C:\ProgramData\app-data" "allow-write=DOMAIN\backup-svc" allow-write=S-1-1-0
L        cli OK: C:\ProgramData\app-data: owner BUILTIN\Administrators, no unexpected write access
```

**Show the whole access control list**

Deny entries are prefixed with `!` and inherited entries with `~`.

```
check_file_security "path=C:\ProgramData\app-data" "top-syntax=${status}: ${list}" "detail-syntax=${path}: ${state} [${dacl}]"
L        cli CRITICAL: C:\ProgramData\app-data: world writable by Everyone, WS01\bob [Everyone(RWXD), ~NT AUTHORITY\SYSTEM(F), ~BUILTIN\Administrators(F), ~WS01\bob(F)]
```

**Report who can write without alerting**

```
check_file_security "path=C:\Windows\System32" warning=none critical=none "top-syntax=${status}: ${list}" "detail-syntax=owner=${owner} writable=${writable} aces=${ace_count}"
L        cli OK: owner=NT SERVICE\TrustedInstaller writable=NT SERVICE\TrustedInstaller, NT AUTHORITY\SYSTEM, BUILTIN\Administrators, CREATOR OWNER aces=13
```

**Check owner, group and permissions (Unix)**

A path is summed up the way `ls -l` shows it. `root` and the `root` group may
write; anybody else holding a write bit is a finding.

```
check_file_security path=/etc/passwd
L        cli OK: /etc/passwd: owner root:root mode 0644, no unexpected write access
```

```
check_file_security path=/etc/myapp/conf.d/extra.conf
L        cli CRITICAL: /etc/myapp/conf.d/extra.conf: world writable by other
```

**Require a specific owner and group (Unix)**

Names and numeric ids both work.

```
check_file_security path=/etc/shadow expected-owner=root expected-group=shadow
L        cli OK: /etc/shadow: owner root:shadow mode 0640, no unexpected write access
```

```
check_file_security path=/etc/shadow expected-group=root
L        cli CRITICAL: /etc/shadow: unexpected group shadow
```

A directory owned by a service account is writable by that account, which is
only a warning until the account is named as the expected owner:

```
check_file_security path=/var/lib/myapp
L        cli WARNING: /var/lib/myapp: writable by user:www-data
```

```
check_file_security path=/var/lib/myapp expected-owner=www-data
L        cli OK: /var/lib/myapp: owner www-data:www-data mode 0750, no unexpected write access
```

**Require an exact mode (Unix)**

```
check_file_security path=/etc/myapp/app.key "critical=mode != '0600'"
L        cli OK: /etc/myapp/app.key: owner root:root mode 0600, no unexpected write access
```

```
check_file_security path=/etc/myapp/app.conf "critical=mode != '0600'" "detail-syntax=${path}: mode ${mode}"
L        cli CRITICAL: /etc/myapp/app.conf: mode 0644
```

**Find set-user-ID programs among a list of paths (Unix)**

```
check_file_security path=/usr/bin/passwd path=/usr/bin/ls warning=none "critical=setuid = 1" "detail-syntax=${path} (${mode})" "top-syntax=${status}: ${problem_list}"
L        cli CRITICAL: /usr/bin/passwd (4755)
```

**Find what a POSIX ACL grants behind the mode bits (Linux)**

After `setfacl -m u:ubuntu:rw` the mode only shows a group write bit; the entry
list names who actually got it.

```
check_file_security path=/etc/myapp/acl.conf "detail-syntax=${path}: ${state} [${dacl}]"
L        cli WARNING: /etc/myapp/acl.conf: writable by user:ubuntu [user:root(rw-), user:ubuntu(rw-), group:root(r--), other(---)]
```

```
check_file_security path=/etc/myapp/acl.conf allow-write=uid:1000
L        cli OK: /etc/myapp/acl.conf: owner root:root mode 0660, no unexpected write access
```

**Accept a directory that is meant to be open**

```
check_file_security path=/tmp
L        cli CRITICAL: /tmp: world writable by other
```

```
check_file_security path=/tmp allow-write=other
L        cli OK: /tmp: owner root:root mode 1777, no unexpected write access
```

**Check everything inside a directory**

`max-depth` adds the contents of a directory, that many levels down. Listing
only the problems keeps the line short however large the tree is.

```
check_file_security path=/etc/myapp max-depth=2 expected-owner=root "top-syntax=${status}: ${problem_count} of ${count} path(s) have a problem: ${problem_list}"
L        cli CRITICAL: 1 of 5 path(s) have a problem: /etc/myapp/conf.d/extra.conf: world writable by other
```

**Check the program a systemd unit runs**

```
check_file_security service=apt-daily "detail-syntax=${service} (${path}): ${state}"
L        cli OK: apt-daily (/usr/lib/apt/apt.systemd.daily): owner root:root mode 0755, no unexpected write access
```

**Report size and timestamps next to the permissions**

```
check_file_security path=/etc/passwd warning=none critical=none "detail-syntax=${owner}:${group} ${mode} ${size} bytes, written ${written} [${dacl}]" "top-syntax=${status}: ${list}"
L        cli OK: root:root 0644 3333 bytes, written 2026-11-16 18:50:16 [user:root(rw-), group:root(r--), other(r--)]
```

Alert when a file that should never change was written recently:

```
check_file_security path=/etc/passwd "warning=written > -30d" "detail-syntax=${path} written ${written}"
L        cli WARNING: /etc/passwd written 2026-11-16 18:50:16
```
