**List the local Administrators group (no allow-list → just enumerate):**

```
check_group_members
OK: MYPC\Administrator (user), MYPC\localadmin (user), PRICER\Domain Admins (group)
```

**Alert if the Administrators group contains anyone unexpected (drift detection):**

```
check_group_members expected=Administrator "expected=Domain Admins" expected=localadmin
OK: All 3 member(s) are on the expected list.
```

**Drift detected — an unexpected member is present:**

```
check_group_members expected=Administrator "expected=Domain Admins"
CRITICAL: MYPC\intern (user)
```

**Check a different group:**

```
check_group_members group="Remote Desktop Users" expected=helpdesk
OK: All 1 member(s) are on the expected list.
```

**Alert if a group should have no direct user members (only groups):**

```
check_group_members group=Administrators "crit=type = 'user'"
OK: All 2 member(s) are on the expected list.
```

**List members with their type and SID:**

```
check_group_members "top-syntax=%(status): %(list)" "detail-syntax=%(member) [%(type)] %(sid)"
OK: MYPC\Administrator [user] S-1-5-21-...-500, PRICER\Domain Admins [group] S-1-5-21-...-512
```

**A group that does not exist is reported as an error:**

```
check_group_members group=NoSuchGroup
UNKNOWN: Local group not found: NoSuchGroup
```

**Over NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_group_members --argument "expected=Administrator" --argument "expected=Domain Admins"
OK: All 2 member(s) are on the expected list.
```
