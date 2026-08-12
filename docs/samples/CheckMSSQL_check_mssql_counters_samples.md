**Default check (informational, all counters emitted as perfdata):**

```
check_mssql_counters
OK: hit ratio 100%, PLE 4010s, 155.819 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=155.81854;0;0 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0 'mssql_hit_ratio'=100%;0;0 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_page_life_expectancy'=4010s;0;0 'mssql_recompilations'=0;0;0
```

The check samples the cumulative counters twice, one second apart, so it takes
about a second longer than the other CheckMSSQL commands.

**Alert on memory-pressure symptoms:**

```
check_mssql_counters "warning=hit_ratio < 95 or page_life_expectancy < 300" "critical=hit_ratio < 85 or page_life_expectancy < 60"
OK: hit ratio 100%, PLE 4012s, 148.368 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=148.36795;0;0 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0 'mssql_hit_ratio'=100%;95;85 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_page_life_expectancy'=4012s;300;60 'mssql_recompilations'=0;0;0
```

**A workload spike trips a batch-rate threshold:**

```
check_mssql_counters "warning=batch_requests > 100" "critical=batch_requests > 10000"
WARNING: hit ratio 100%, PLE 4061s, 154.303 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=154.30267;100;10000 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0 'mssql_hit_ratio'=100%;0;0 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_page_life_expectancy'=4061s;0;0 'mssql_recompilations'=0;0;0
```

**Watch locking health (pairs with `check_mssql_blocking`):**

```
check_mssql_counters "warning=lock_waits > 50" "critical=deadlocks > 0.5"
OK: hit ratio 100%, PLE 4102s, 0 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=0;0;0 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0.5 'mssql_hit_ratio'=100%;0;0 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;50;0 'mssql_page_life_expectancy'=4102s;0;0 'mssql_recompilations'=0;0;0
```
