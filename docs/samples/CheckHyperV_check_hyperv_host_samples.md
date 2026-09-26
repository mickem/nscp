**Check the host with the default thresholds (critical when any VM is unhealthy):**

```
check_hyperv_host
OK: 12 VMs ok, 0 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_health_ok'=12;0;0 'vms_health_critical'=0;0;0 'vms_logical_processors'=32;0;0 'vms_virtual_processors'=48;0;0 'vms_partitions'=13;0;0
```

**Warn when the host is over-committed on processors:**

```
check_hyperv_host "warning=virtual_processors > 64" "critical=health_critical > 0"
OK: 12 VMs ok, 0 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_virtual_processors'=48;64;0 'vms_health_critical'=0;0;0 'vms_health_ok'=12;0;0 'vms_logical_processors'=32;0;0 'vms_partitions'=13;0;0
```

**A VM the host cannot keep running trips the default critical threshold:**

```
check_hyperv_host
CRITICAL: 11 VMs ok, 1 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_health_critical'=1;0;0 'vms_health_ok'=11;0;0 'vms_logical_processors'=32;0;0 'vms_virtual_processors'=48;0;0 'vms_partitions'=13;0;0
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_host
Hyper-V counters (Hyper-V Virtual Machine Health Summary, Hyper-V Hypervisor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```
