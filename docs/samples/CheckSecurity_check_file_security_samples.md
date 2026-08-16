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

**On non-Windows platforms**

```
check_file_security "path=/etc/nsclient"
L        cli UNKNOWN: check_file_security is not supported on this platform (Windows security descriptors only)
```
