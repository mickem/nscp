**Check all virtual machines with the default thresholds:**

```
check_hyperv_vms
OK: all 3 virtual machine(s) ok|'web-01_uptime'=864000s;0;0 'web-01_memory_assigned'=4294967296B;0;0 'web-01_cpu_load'=12%;0;0 'db-01_uptime'=864012s;0;0 'db-01_memory_assigned'=17179869184B;0;0 'db-01_cpu_load'=41%;0;0 'test-01_uptime'=0s;0;0 'test-01_memory_assigned'=0B;0;0 'test-01_cpu_load'=0%;0;0
```

**A running guest that stops answering the heartbeat (a hung VM):**

```
check_hyperv_vms
WARNING: web-01: running, heartbeat lost_communication, health ok|'web-01_uptime'=864000s;0;0 'web-01_memory_assigned'=4294967296B;0;0 'web-01_cpu_load'=100%;0;0 ...
```

**Only the VMs that are supposed to be running, and alert when one is not:**

```
check_hyperv_vms "filter=vm like 'prod-'" "critical=state != 'running'"
CRITICAL: prod-db-02: off, heartbeat disabled, health ok|'prod-db-02_uptime'=0s;0;0 'prod-db-02_memory_assigned'=0B;0;0 'prod-db-02_cpu_load'=0%;0;0 ...
```

**Forgotten checkpoints older than a week:**

```
check_hyperv_vms "warning=oldest_snapshot < -7d" "detail-syntax=${vm}: ${snapshots} checkpoint(s), oldest ${oldest_snapshot}"
WARNING: db-01: 2 checkpoint(s), oldest 2026-08-30 14:02:11|...
```

**Hyper-V Replica health on a primary:**

```
check_hyperv_vms "filter=replication_mode != 'none'" "warning=replication_health = 'warning'" "critical=replication_health = 'critical'" "detail-syntax=${vm}: ${replication_state} (${replication_health})"
OK: all 2 virtual machine(s) ok|...
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_vms
Hyper-V virtual machine information not available: the Hyper-V role is not installed on this host (root\virtualization\v2 missing)
```

**Run by an account that may not see the virtual machines (here: `nscp test` in a shell that is not elevated):**

```
check_hyperv_vms
Hyper-V reports 1 virtual machine(s) on this host but none are visible to this account: run as an elevated administrator or a member of Hyper-V Administrators
```
