**Check all virtual switches with the default thresholds:**

```
check_hyperv_switches
OK: all 3 virtual switch(es) ok|'LAN_ports'=9;0;0 'Internal_ports'=2;0;0 'Isolated_ports'=0;0;0
```

**List every switch with its type, whether problems or not:**

```
check_hyperv_switches "top-syntax=${status}: ${list}"
OK: LAN: external, enabled, health ok, Internal: internal, enabled, health ok, Isolated: private, enabled, health ok|'LAN_ports'=9;0;0 'Internal_ports'=2;0;0 'Isolated_ports'=0;0;0
```

**The external switch must exist and be bound to a physical adapter:**

```
check_hyperv_switches "filter=switch = 'LAN'" "critical=type != 'external' or health != 'ok'" empty-state=critical "empty-syntax=CRITICAL: switch LAN is missing"
CRITICAL: switch LAN is missing
```

**Warn when an external switch is not shared with the host (no management access over it):**

```
check_hyperv_switches "filter=type = 'external'" "warning=management_os = 0"
WARNING: LAN: external, enabled, health ok|'LAN_ports'=9;0;0
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_switches
Hyper-V virtual switch information not available: the Hyper-V role is not installed on this host (root\virtualization\v2 missing)
```
