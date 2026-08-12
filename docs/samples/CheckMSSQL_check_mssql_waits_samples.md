**Default check (informational, full wait profile as perfdata):**

```
check_mssql_waits
OK: 0 runnable tasks on 16 schedulers, 0 queued; waits ms/s: cpu 0, io 0, log 28.6807, lock 0, memory 0, signal 9.375%|'mssql_cpu_waits'=0;0;0 'mssql_io_waits'=0;0;0 'mssql_latch_waits'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_log_waits'=28.68068;0;0 'mssql_memory_waits'=0;0;0 'mssql_network_waits'=0;0;0 'mssql_other_waits'=1.91204;0;0 'mssql_runnable_tasks'=0;0;0 'mssql_signal_wait_pct'=9.375%;0;0 'mssql_total_waits'=30.59273;0;0 'mssql_work_queue'=0;0;0 'mssql_workers'=44;0;0
```

Here a write-heavy workload shows up as transaction-log waits (`WRITELOG`,
~29 ms of wait per second) while every other category is quiet — a storage
question, not a locking or CPU one. The check samples the cumulative wait
statistics twice, one second apart, so it takes about a second longer than the
other CheckMSSQL commands.

**Alert on CPU pressure and worker starvation:**

```
check_mssql_waits "warning=work_queue > 0 or signal_wait_pct > 25" "critical=work_queue > 10"
OK: 0 runnable tasks on 16 schedulers, 0 queued; waits ms/s: cpu 0, io 0, log 23.8569, lock 0, memory 0, signal 7.40741%|'mssql_cpu_waits'=0;0;0 'mssql_io_waits'=0;0;0 'mssql_latch_waits'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_log_waits'=23.85685;0;0 'mssql_memory_waits'=0;0;0 'mssql_network_waits'=0;0;0 'mssql_other_waits'=2.9821;0;0 'mssql_runnable_tasks'=0;0;0 'mssql_signal_wait_pct'=7.4074%;25;0 'mssql_total_waits'=26.83896;0;0 'mssql_work_queue'=0;0;10 'mssql_workers'=44;0;0
```

**Alert on storage pressure:**

```
check_mssql_waits "warning=io_waits > 500 or log_waits > 200" "critical=io_waits > 2000"
OK: 0 runnable tasks on 16 schedulers, 0 queued; waits ms/s: cpu 0, io 0, log 29.8211, lock 0, memory 0, signal 6.45161%|'mssql_cpu_waits'=0;0;0 'mssql_io_waits'=0;500;2000 'mssql_latch_waits'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_log_waits'=29.82107;200;0 'mssql_memory_waits'=0;0;0 'mssql_network_waits'=0;0;0 'mssql_other_waits'=0.99403;0;0 'mssql_runnable_tasks'=0;0;0 'mssql_signal_wait_pct'=6.45161%;0;0 'mssql_total_waits'=30.8151;0;0 'mssql_work_queue'=0;0;0 'mssql_workers'=45;0;0
```
