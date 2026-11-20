**Check the host with the default thresholds (critical when any VM is unhealthy):**

```
check_hyperv_host
OK: 1 VMs ok, 0 critical, 3 partitions on 16 logical processors (40 virtual)|'vms_cpu_overcommit'=50% 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9% 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Warn when the running VMs have more virtual processors than the host has logical ones, critical at four times:**

```
check_hyperv_host "warning=cpu_overcommit > 100" "critical=cpu_overcommit > 400 or health_critical > 0" "detail-syntax=${vm_vcpus} vCPUs on ${logical_processors} logical processors (${cpu_overcommit}%)"
OK: 8 vCPUs on 16 logical processors (50%)|'vms_cpu_overcommit'=50%;100;400 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9% 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Warn when the VMs hold more than 90 % of the host's memory:**

```
check_hyperv_host "warning=memory_overcommit > 90" "detail-syntax=${memory_assigned} of ${physical_memory} assigned to ${running_vms} VMs (${memory_overcommit}%)"
OK: 3686793216 of 33777467392 assigned to 1 VMs (10.9%)|'vms_cpu_overcommit'=50% 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9%;90 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Alert on over-commitment, and also when the virtual machines could not be read (`guests_known`):**

```
check_hyperv_host "warning=cpu_overcommit > 300 or guests_known = 0" "critical=health_critical > 0 or memory_overcommit > 95"
OK: 1 VMs ok, 0 critical, 3 partitions on 16 logical processors (40 virtual)|'vms_cpu_overcommit'=50%;300 'vms_guests_known'=1;0 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9%;;95 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Show why the guests could not be read, here for an account without Hyper-V rights while a VM runs:**

```
check_hyperv_host "detail-syntax=${health_ok} VMs ok, guests known: ${guests_known} ${guests_error}"
OK: 1 VMs ok, guests known: 0 Hyper-V reports 1 virtual machine(s) on this host but none are visible to this account: run as an elevated administrator or a member of Hyper-V Administrators|'vms_cpu_overcommit'=0% 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=0% 'vms_partitions'=3 'vms_virtual_processors'=40
```

**A VM the host cannot keep running trips the default critical threshold (illustrative output):**

```
check_hyperv_host
CRITICAL: 0 VMs ok, 1 critical, 2 partitions on 16 logical processors (40 virtual)|'vms_cpu_overcommit'=50% 'vms_health_critical'=1;;0 'vms_health_ok'=0 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9% 'vms_partitions'=2 'vms_virtual_processors'=40
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_host
Hyper-V counters (Hyper-V Virtual Machine Health Summary, Hyper-V Hypervisor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```
