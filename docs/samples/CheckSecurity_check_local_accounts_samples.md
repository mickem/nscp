**Default hygiene check:**

```
check_local_accounts
OK: All 5 local account(s) ok.
```

**A finding — an enabled account with no password required (default CRITICAL):**

```
check_local_accounts
CRITICAL: kiosk (enabled=1, pw_req=0, pw_exp=0, locked=0)
```

**Alert if the built-in Administrator account is enabled (hardening baseline):**

```
check_local_accounts "crit=is_builtin_admin = 1 and enabled = 1"
OK: All 5 local account(s) ok.
```

**Alert on enabled accounts whose password never expires:**

```
check_local_accounts "warn=enabled = 1 and password_expires = 0"
WARNING: svc_backup (enabled=1, pw_req=1, pw_exp=0, locked=0)
```

**Report locked-out accounts:**

```
check_local_accounts "filter=locked = 1" "warn=count > 0" "empty-state=ok"
OK: No local accounts found
```

**List every local account with its flags:**

```
check_local_accounts "warn=none" "crit=none" "top-syntax=%(status): %(list)" "detail-syntax=%(name) enabled=%(enabled) pw_req=%(password_required) pw_exp=%(password_expires)"
OK: Administrator enabled=0 pw_req=1 pw_exp=0, Guest enabled=0 pw_req=0 pw_exp=0, ...
```

**Over NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_local_accounts --argument "crit=enabled = 1 and password_required = 0"
OK: All 5 local account(s) ok.
```

**Linux: the accounts that can log on interactively**

The default thresholds apply; the system accounts with `nologin` shells and
`*` / `!` password fields are filtered out here, and would be OK anyway:

```
check_local_accounts "filter=interactive = 1"
OK: root (enabled=1, pw_req=1, pw_exp=0, locked=0), claude (enabled=1, pw_req=1, pw_exp=0, locked=1), ubuntu (enabled=1, pw_req=1, pw_exp=0, locked=1), postgres (enabled=1, pw_req=1, pw_exp=0, locked=1)|'root_enabled'=1;1;1 'root_is_builtin_guest'=0;1 'root_password_required'=1;;0 'claude_enabled'=1;1;1 'claude_is_builtin_guest'=0;1 'claude_password_required'=1;;0 'ubuntu_enabled'=1;1;1 'ubuntu_is_builtin_guest'=0;1 'ubuntu_password_required'=1;;0 'postgres_enabled'=1;1;1 'postgres_is_builtin_guest'=0;1 'postgres_password_required'=1;;0 'count'=4
```

**Linux: inventory with the POSIX keywords**

```
check_local_accounts "filter=interactive = 1" "top-syntax=%(status): %(list)" "detail-syntax=%(name) uid=%(uid) shell=%(shell) pw=%(password_state)" "crit=none" "warn=none"
OK: root uid=0 shell=/bin/bash pw=none, claude uid=999 shell=/bin/bash pw=locked, ubuntu uid=1000 shell=/bin/bash pw=locked, postgres uid=102 shell=/bin/bash pw=locked|'count'=4
```

**Linux: a second superuser (UID 0 other than root)**

```
check_local_accounts "crit=is_extra_uid0 = 1" "filter=uid = 0"
OK: root (enabled=1, pw_req=1, pw_exp=0, locked=0)|'root_enabled'=1;1 'root_is_builtin_guest'=0;1 'root_is_extra_uid0'=0;;1 'count'=1
```

**Linux: password age**

`password_age` takes durations; `password_state = 'set'` limits the rule to
accounts that actually have a password:

```
check_local_accounts "filter=interactive = 1" "warn=password_state = 'set' and password_age > 90d" "crit=password_state = 'empty'"
OK: root (enabled=1, pw_req=1, pw_exp=0, locked=0), claude (enabled=1, pw_req=1, pw_exp=0, locked=1), ubuntu (enabled=1, pw_req=1, pw_exp=0, locked=1), postgres (enabled=1, pw_req=1, pw_exp=0, locked=1)|'root_password_age'=382842s;7776000 'claude_password_age'=382842s;7776000 'ubuntu_password_age'=382842s;7776000 'postgres_password_age'=382842s;7776000 'count'=4
```

**Linux: the agent cannot read `/etc/shadow`**

Run as an unprivileged user, the default check refuses to report OK on
password state it could not see (UNKNOWN):

```
check_local_accounts
Cannot read /etc/shadow: Permission denied - password and expiry state is unknown; run the agent with read access to it or pass allow-unreadable-shadow
```

Accepting the gap keeps the `/etc/passwd` keywords usable; the shadow-derived
ones render `unknown`:

```
check_local_accounts allow-unreadable-shadow=true "filter=uid = 0 or interactive = 1"
OK: root (enabled=unknown, pw_req=unknown, pw_exp=unknown, locked=unknown), claude (enabled=unknown, pw_req=unknown, pw_exp=unknown, locked=unknown), ubuntu (enabled=unknown, pw_req=unknown, pw_exp=unknown, locked=unknown), postgres (enabled=unknown, pw_req=unknown, pw_exp=unknown, locked=unknown)|'root_is_builtin_guest'=0;1 'claude_is_builtin_guest'=0;1 'ubuntu_is_builtin_guest'=0;1 'postgres_is_builtin_guest'=0;1 'count'=4
```
