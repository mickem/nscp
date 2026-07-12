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
