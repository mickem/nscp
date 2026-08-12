**Default check:**

```
check_cpu_frequency
OK: Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz: 2900/4800 MHz (60%)
'Intel...current_mhz'=2900MHz;;; 'Intel...max_mhz'=4800MHz;;; 'Intel...frequency_pct'=60%;;; 'Intel...load_pct'=12%;;;
```

**Per-socket filtering and load:**

`Win32_Processor` returns one row per physical CPU socket, exposed via `socket_id`
(DeviceID, e.g. `CPU0`) and `socket` (SocketDesignation, e.g. `CPU 1`). The
`load_pct` keyword reports `Win32_Processor.LoadPercentage` per socket.

```
check_cpu_frequency "filter=socket_id = 'CPU0'" "warn=load_pct > 90" "detail-syntax=${socket}: ${load_pct}% @ ${current_mhz}MHz"
OK: CPU 1: 12% @ 2900MHz
'Intel...load_pct'=12%;90;;
```

**CPU hardware inventory (model, architecture, cores/threads, cache):**

```
check_cpu_frequency "detail-syntax=${name}: ${architecture}, ${cores}c/${logical_processors}t, L2 ${l2_cache}, L3 ${l3_cache}"
OK: Intel(R) Core(TM) Ultra 7 265H: x64, 16c/16t, L2 28MB, L3 24MB
```

**Pin expected hardware (re-imaged / migrated box detection):**

```
check_cpu_frequency "warn=l3_cache < 1M" "crit=architecture != 'x64'"
OK: Intel(R) Core(TM) Ultra 7 265H: 2200/2200 MHz (100%)
```
