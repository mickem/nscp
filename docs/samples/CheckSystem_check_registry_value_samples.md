**Read a single value (default: enumerates all values in the key):**

```
check_registry_value "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" value=ProductName
OK: HKLM\Software\Microsoft\Windows NT\CurrentVersion\ProductName: Windows 10 Pro (type=REG_SZ)
```

**Read multiple specific values from the same key:**

```
check_registry_value "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" value=ProductName value=CurrentBuild value=ReleaseId
OK: All 3 registry value(s) are ok.
```

**Enumerate every value in a key:**

```
check_registry_value "key=HKLM\Software\NSClient" "top-syntax=%(status): %(list)" "detail-syntax=%(name)=%(string_value)"
OK: ConfigFile=C:\Program Files\NSClient++\nsclient.ini, InstallVersion=0.6.0, ...
```

**Value that does not exist (default `crit=not exists`):**

```
check_registry_value "key=HKLM\Software\NSClient" value=NoSuchValue
CRITICAL: HKLM\Software\NSClient\NoSuchValue: (type=REG_NONE)
```

**Type assertion (alert if a value isn't the expected type):**

```
check_registry_value "key=HKLM\Software\NSClient" value=InstallVersion "crit=type != 'REG_SZ' or not exists"
OK: HKLM\Software\NSClient\InstallVersion: 0.6.0 (type=REG_SZ)
```

**Numeric DWORD / QWORD comparison:**

```
check_registry_value "key=HKLM\System\CurrentControlSet\Services\W32Time\Config" value=MaxPollInterval "warn=int_value > 14" "crit=int_value > 17"
OK: HKLM\System\CurrentControlSet\Services\W32Time\Config\MaxPollInterval: 10 (type=REG_DWORD)
```

**String / content match:**

```
check_registry_value "key=HKLM\Software\NSClient" value=ConfigFile "crit=string_value not like 'C:\\Program Files\\NSClient++\\nsclient.ini'"
OK: HKLM\Software\NSClient\ConfigFile: C:\Program Files\NSClient++\nsclient.ini (type=REG_SZ)
```

**Size watchdog (alert if a binary blob grows unexpectedly):**

```
check_registry_value "key=HKLM\Software\NSClient" value=Cache "warn=size > 4096" "crit=size > 16384"
OK: HKLM\Software\NSClient\Cache: 0xDEADBEEF... (type=REG_BINARY)
```

**Force the 32-bit registry view (WoW64):**

```
check_registry_value "key=HKLM\Software\NSClient" value=InstallDir view=32
OK: HKLM\Software\NSClient\InstallDir: C:\Program Files (x86)\NSClient++\ (type=REG_SZ)
```

**Recursive enumeration of values across an entire sub-tree:**

```
check_registry_value "key=HKLM\Software\NSClient" recursive max-depth=2 "top-syntax=%(status): %(list)" "detail-syntax=%(path)=%(string_value)"
OK: HKLM\Software\NSClient\ConfigFile=..., HKLM\Software\NSClient\modules\enabled=1, ...
```

**Exclude noisy values during enumeration:**

```
check_registry_value "key=HKCU\Software\NSClient" exclude=LastRun exclude=Cache "warn=value_count < 3"
OK: All 5 registry value(s) are ok.
```

**Custom output text including type / size:**

```
check_registry_value "key=HKLM\Software\NSClient" value=InstallVersion "top-syntax=%(status): %(list)" "detail-syntax=%(name) [%(type)] = %(string_value) (%(size)B)"
OK: InstallVersion [REG_SZ] = 0.6.0 (12B)
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_registry_value --argument "key=HKLM\Software\NSClient" --argument "value=InstallVersion"
OK: HKLM\Software\NSClient\InstallVersion: 0.6.0 (type=REG_SZ)
```
