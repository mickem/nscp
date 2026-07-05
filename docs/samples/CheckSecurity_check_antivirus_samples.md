**Check that antivirus is enabled and up to date (Windows)**

The default is critical if any registered product has real-time protection off
or stale definitions.

```
check_antivirus
L        cli OK: 1 antivirus product(s) healthy
```

```
check_antivirus
L        cli CRITICAL: Windows Defender (enabled=1 up_to_date=0)
```

**Only require definitions to be current**

```
check_antivirus "crit=up_to_date = 0"
L        cli OK: 1 antivirus product(s) healthy
```

**Show each product's state**

```
check_antivirus "top-syntax=${list}" "detail-syntax=${name}: enabled=${enabled} current=${up_to_date} state=${product_state}"
L        cli OK: Windows Defender: enabled=1 current=1 state=397568
```

**On non-Windows platforms**

```
check_antivirus
L        cli UNKNOWN: check_antivirus is not supported on this platform (Windows Security Center only)
```
