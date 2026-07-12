**Default check (healthy Defender):**

```
check_defender
OK: Defender enabled=1 realtime=1 tamper=0 sig_age=0d sig=1.455.84.0 engine=1.1.26060.3008
```

**Alert only on protection state, ignore signature age:**

```
check_defender "warning=none" "critical=enabled = 0 or realtime_enabled = 0"
OK: Defender enabled=1 realtime=1 tamper=0 sig_age=0d sig=1.455.84.0 engine=1.1.26060.3008
```

**Require tamper protection to be on:**

```
check_defender "critical=tamper_protection = 0"
CRITICAL: Defender enabled=1 realtime=1 tamper=0 sig_age=0d sig=1.455.84.0 engine=1.1.26060.3008
```

**Tighten the signature-age thresholds (warn at 1 day, critical at 3):**

```
check_defender "warning=signature_age > 1" "critical=signature_age > 3"
WARNING: Defender enabled=1 realtime=1 tamper=1 sig_age=2d sig=1.455.60.0 engine=1.1.26060.3008
```

**Also alert if no quick scan has run in the last week:**

```
check_defender "warning=signature_age > 3 or quick_scan_age > 7"
OK: Defender enabled=1 realtime=1 tamper=1 sig_age=0d sig=1.455.84.0 engine=1.1.26060.3008
```

**Custom output listing scan ages and versions:**

```
check_defender "top-syntax=%(status): %(list)" "detail-syntax=sig=%(signature_age)d quick=%(quick_scan_age)d full=%(full_scan_age)d engine=%(engine_version)"
OK: sig=0d quick=6d full=-1d engine=1.1.26060.3008
```

**Where a third-party antivirus is the active product (Defender status unavailable):**

```
check_defender
UNKNOWN: Microsoft Defender status unavailable (not installed or another antivirus is active)
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_defender --argument "warning=signature_age > 2"
OK: Defender enabled=1 realtime=1 tamper=1 sig_age=0d sig=1.455.84.0 engine=1.1.26060.3008
```
