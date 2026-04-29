**Default check (single key, just verifies it exists):**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion"
OK: All 1 registry key(s) are ok.
```

**Key that does not exist (default `crit=not exists`):**

```
check_registry_key "key=HKLM\Software\DoesNotExist"
CRITICAL: HKLM\Software\DoesNotExist: exists=false, subkeys=0, values=0
```

**Check several keys in one call:**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" "key=HKLM\Software\NSClient"
OK: All 2 registry key(s) are ok.
```

**Wildcard / recursive enumeration of immediate sub-keys:**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" recursive max-depth=1 "top-syntax=%(status): %(list)" "detail-syntax=%(name) (subkeys=%(subkey_count), values=%(value_count))"
OK: AeDebug (subkeys=1, values=2), Compatibility32 (subkeys=0, values=0), Console (subkeys=4, values=18), ...
```

**Force a 32-bit or 64-bit registry view (WoW64):**

```
check_registry_key "key=HKLM\Software\NSClient" view=64
OK: All 1 registry key(s) are ok.

check_registry_key "key=HKLM\Software\NSClient" view=32
CRITICAL: HKLM\Software\NSClient: exists=false, subkeys=0, values=0
```

**Exclude noisy sub-keys when recursing:**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall" recursive max-depth=1 exclude=KB5005463 exclude=KB5005539
OK: All 248 registry key(s) are ok.
```

**Alert when a key is unexpectedly empty:**

```
check_registry_key "key=HKLM\Software\NSClient" "warn=value_count < 5" "crit=value_count = 0 or not exists"
OK: HKLM\Software\NSClient: exists=true, subkeys=2, values=12
```

**Alert when a key has not been written for over 30 days (configuration drift watchdog):**

```
check_registry_key "key=HKLM\Software\NSClient" "warn=age > 7d" "crit=age > 30d or not exists"
OK: HKLM\Software\NSClient: exists=true, subkeys=2, values=12
```

**Custom output text:**

```
check_registry_key "key=HKLM\Software\NSClient" "top-syntax=%(status): %(list)" "detail-syntax=%(path) last-written %(written_s)"
OK: HKLM\Software\NSClient last-written 2026-04-15 09:12:33
```

**Remote computer / 32-bit view via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_registry_key --argument "key=HKLM\Software\NSClient" --argument "view=64"
OK: All 1 registry key(s) are ok.
```
