**Check the host with the default thresholds (critical when any VM is unhealthy):**

```
check_hyperv_host
OK: 12 VMs ok, 0 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_health_ok'=12;0;0 'vms_health_critical'=0;0;0 'vms_logical_processors'=32;0;0 'vms_virtual_processors'=48;0;0 'vms_partitions'=13;0;0 'vms_cpu_overcommit'=50%;0;0 'vms_memory_overcommit'=62.5%;0;0
```

**Warn when the running VMs have more virtual processors than the host has logical ones, critical at four times:**

```
check_hyperv_host "warning=cpu_overcommit > 100" "critical=cpu_overcommit > 400 or health_critical > 0" "top-syntax=${status}: ${vm_vcpus} vCPUs on ${logical_processors} logical processors (${cpu_overcommit}%)"
WARNING: 40 vCPUs on 32 logical processors (125%)|'vms_cpu_overcommit'=125%;100;400 'vms_health_ok'=12;0;0 'vms_health_critical'=0;0;0 'vms_logical_processors'=32;0;0 'vms_virtual_processors'=48;0;0 'vms_partitions'=13;0;0 'vms_memory_overcommit'=62.5%;0;0
```

**Warn when the VMs hold more than 90 % of the host's memory:**

```
check_hyperv_host "warning=memory_overcommit > 90" "top-syntax=${status}: ${memory_assigned} of ${physical_memory} assigned to ${running_vms} VMs (${memory_overcommit}%)"
OK: 171798691840 of 274877906944 assigned to 12 VMs (62.5%)|'vms_memory_overcommit'=62.5%;90;0 ...
```

**A VM the host cannot keep running trips the default critical threshold:**

```
check_hyperv_host
CRITICAL: 11 VMs ok, 1 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_health_critical'=1;0;0 'vms_health_ok'=11;0;0 'vms_logical_processors'=32;0;0 'vms_virtual_processors'=48;0;0 'vms_partitions'=13;0;0 'vms_cpu_overcommit'=50%;0;0 'vms_memory_overcommit'=62.5%;0;0
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_host
Hyper-V counters (Hyper-V Virtual Machine Health Summary, Hyper-V Hypervisor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```
