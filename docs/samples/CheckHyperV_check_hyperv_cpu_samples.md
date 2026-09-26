**Check the aggregate logical processor load with the default thresholds (80/90 %):**

```
check_hyperv_cpu
OK: total: 23.4% total (21.1% guest, 2.3% hypervisor)|'total_total_run_time'=23.4%;80;90 'total_guest_run_time'=21.1%;0;0 'total_hypervisor_run_time'=2.3%;0;0
```

**Alert on individual logical processors instead of the average:**

```
check_hyperv_cpu "filter=processor != 'total'" "warning=total_run_time > 90" "critical=total_run_time > 98"
WARNING: Hv LP 5: 94.2% total (93.1% guest, 1.1% hypervisor)|'Hv LP 0_total_run_time'=12.5%;90;98 'Hv LP 0_guest_run_time'=11.9%;0;0 'Hv LP 0_hypervisor_run_time'=0.6%;0;0 'Hv LP 1_total_run_time'=9.3%;90;98 ...
```

**Watch the hypervisor's own share of the time:**

```
check_hyperv_cpu "warning=hypervisor_run_time > 15" "critical=hypervisor_run_time > 30"
OK: total: 31.0% total (27.2% guest, 3.8% hypervisor)|'total_hypervisor_run_time'=3.8%;15;30 'total_total_run_time'=31%;0;0 'total_guest_run_time'=27.2%;0;0
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_cpu
Hyper-V counters (Hyper-V Hypervisor Logical Processor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```
