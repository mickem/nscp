**Check all virtual switches with the default thresholds:**

```
check_hyperv_switches
OK: all 2 virtual switch(es) ok|'Default Switch_ports'=2 'FSE Switch_ports'=6
```

**List every switch with its type and state, whether problems or not:**

```
check_hyperv_switches "top-syntax=${status}: ${list}" "ok-syntax=${status}: ${list}"
OK: Default Switch: internal, not_applicable, health ok, FSE Switch: internal, not_applicable, health ok|'Default Switch_ports'=2 'FSE Switch_ports'=6
```

**A switch the host must have; critical when it is missing:**

```
check_hyperv_switches "filter=switch = 'LAN'" "critical=type != 'external' or health != 'ok'" empty-state=critical "empty-syntax=CRITICAL: switch LAN is missing"
CRITICAL: switch LAN is missing
```

**Ports per switch, and whether the host has an adapter on it:**

```
check_hyperv_switches "warning=ports > 100" "ok-syntax=${status}: ${list}" "detail-syntax=${switch}: ${type}, ${ports} ports, host adapter ${management_os}"
OK: Default Switch: internal, 2 ports, host adapter 1, FSE Switch: internal, 6 ports, host adapter 1|'Default Switch_ports'=2;100 'FSE Switch_ports'=6;100
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_switches
Hyper-V virtual switch information not available: the Hyper-V role is not installed on this host (root\virtualization\v2 missing)
```
