**Check CAL key packs on an RD licensing server (defaults: warn below 10 available, critical when exhausted):**

```
check_rds_licenses
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=25;0;0 'RDS Per User CAL_available'=25;10;0
```

**A key pack running out of licenses trips the default thresholds:**

```
check_rds_licenses
CRITICAL: RDS Per User CAL: 50/50 issued, 0 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=50;0;0 'RDS Per User CAL_available'=0;10;0
```

**Custom thresholds, e.g. warn when more than 80% of a pack is issued:**

```
check_rds_licenses "warning=issued > 40 and total_licenses > 0" "critical=available = 0 and total_licenses > 0"
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_issued'=25;40;0 'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_available'=25;0;0
```

**Scope the check to a product version or exclude the built-in pack:**

```
check_rds_licenses "filter=product_version like '2022' and type != 'built-in'"
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=25;0;0 'RDS Per User CAL_available'=25;10;0
```

**On a host without the RD Licensing role the check reports UNKNOWN with a clear message:**

```
check_rds_licenses
Remote Desktop licensing information not available: the Remote Desktop licensing role is not installed (Win32_TSLicenseKeyPack missing)
```
