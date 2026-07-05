**Check that all volumes are BitLocker-protected (Windows)**

The default is critical if any encryptable volume is not protected.

```
check_bitlocker
L        cli OK: all 2 volume(s) protected
```

```
check_bitlocker
L        cli CRITICAL: D: protected=0
```

**Only require the system drive to be protected**

```
check_bitlocker "filter=drive = 'C:'" "crit=protected = 0"
L        cli OK: all 1 volume(s) protected
```

**Show each volume's protection state**

```
check_bitlocker "top-syntax=${list}" "detail-syntax=${drive} protected=${protected} status=${protection_status}"
L        cli OK: C: protected=1 status=1, D: protected=1 status=1
```

**On non-Windows platforms**

```
check_bitlocker
L        cli UNKNOWN: check_bitlocker is not supported on this platform (Windows BitLocker only)
```
