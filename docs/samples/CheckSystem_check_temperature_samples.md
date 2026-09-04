**A host with no readable sensors:**

Most VMs and cloud instances expose no thermal zones at all. The check returns
UNKNOWN before the filter runs, rather than a misleading OK.

```
check_temperature
UNKNOWN: No temperature sensors found
```

**Default check on hardware that does report (`> 70` warns, `> 90` is critical):**

```
check_temperature
OK: thermal_zone0: 42 C, thermal_zone1: 38 C|'thermal_zone0'=42;70;90 'thermal_zone1'=38;70;90
```

**Fleet-wide thresholds:**

Sensor names differ between machines, vendors and kernel versions, so threshold
across all of them and let `detail-syntax` name the offender rather than
hard-coding a zone.

```
check_temperature "warn=temperature > 75" "crit=temperature > 90" "detail-syntax=${name}=${temperature}C"
CRITICAL: coretemp Package id 0=94C
```

**Watch one specific sensor:**

```
check_temperature "filter=name like 'Package'" "crit=temperature > 85"
OK: coretemp Package id 0: 61 C
```

**Only the zones that are currently active:**

```
check_temperature "filter=active = 1" "detail-syntax=${name}=${temperature}C"
OK: thermal_zone0=42C
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_temperature --arguments "crit=temperature > 90"
OK: thermal_zone0: 42 C, thermal_zone1: 38 C
```
