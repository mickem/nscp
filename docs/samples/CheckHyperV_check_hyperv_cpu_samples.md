**Check the aggregate logical processor load with the default thresholds (80/90 %):**

```
check_hyperv_cpu
OK: total: 23% total (21% guest, 2% hypervisor)|'total'=23%;80;90 'total_guest'=21%;0;0 'total_hypervisor'=2%;0;0
```

**Alert on individual logical processors instead of the average:**

```
check_hyperv_cpu "filter=processor != 'total'" "warning=total_run_time > 90" "critical=total_run_time > 98"
WARNING: Hv LP 5: 94% total (93% guest, 1% hypervisor)|'Hv LP 0'=12%;90;98 'Hv LP 0_guest'=11%;0;0 'Hv LP 0_hypervisor'=1%;0;0 'Hv LP 1'=9%;90;98 ...
```

**Watch the hypervisor's own share of the time:**

```
check_hyperv_cpu "warning=hypervisor_run_time > 15" "critical=hypervisor_run_time > 30"
OK: total: 31% total (27% guest, 4% hypervisor)|'total_hypervisor'=4%;15;30 'total'=31%;0;0 'total_guest'=27%;0;0
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_cpu
Hyper-V counters (Hyper-V Hypervisor Logical Processor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```
