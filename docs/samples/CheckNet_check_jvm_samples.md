**Check the heap, threads and garbage collection of a JVM running the Jolokia agent on its default port:**

```
check_jvm
OK: ok: heap 298MB of 1GB (29.1%), 42 threads, 123 collections|'127.0.0.1_gc_count'=123c '127.0.0.1_gc_time'=834ms '127.0.0.1_heap_usage'=29.1%;80;90 '127.0.0.1_heap_used'=312475648B '127.0.0.1_threads'=42
```

**Set the heap thresholds with the short options:**

```
check_jvm w=25 c=50
WARNING: ok: heap 298MB of 1GB (29.1%), 42 threads, 123 collections|'127.0.0.1_gc_count'=123c '127.0.0.1_gc_time'=834ms '127.0.0.1_heap_usage'=29.1%;25;50 '127.0.0.1_heap_used'=312475648B '127.0.0.1_threads'=42
```

**Show the JVM version and its garbage collectors:**

```
check_jvm "detail-syntax=${vm_name} ${vm_version} (Java ${spec_version}) up ${uptime}s, collectors: ${collectors}"
OK: OpenJDK 64-Bit Server VM 21.0.5+11-LTS (Java 21) up 93723s, collectors: G1 Young Generation, G1 Concurrent GC, G1 Old Generation|'127.0.0.1_gc_count'=123c '127.0.0.1_gc_time'=834ms '127.0.0.1_heap_usage'=29.1%;80;90 '127.0.0.1_heap_used'=312475648B '127.0.0.1_threads'=42
```

**Alert on the active sessions of a Tomcat web application (note the `!/` escape for the context path):**

```
check_jvm "attribute=sessions=Catalina:type=Manager,host=localhost,context=!/app/activeSessions" "warning=sessions > 100" "detail-syntax=${sessions} sessions, heap ${heap}"
WARNING: 118 sessions, heap 298MB of 1GB|'127.0.0.1_gc_count'=123c '127.0.0.1_gc_time'=834ms '127.0.0.1_heap_usage'=29.1%;;90 '127.0.0.1_heap_used'=312475648B '127.0.0.1_sessions'=118;100 '127.0.0.1_threads'=42
```

**Read one field of a composite attribute (the old generation's usage) with an inner path. Keep `result != 'ok'` in your own `critical`, or a JVM that is down reads as OK:**

```
check_jvm "attribute=old_gen=java.lang:type=MemoryPool,name=G1 Old Gen/Usage/used" "critical=result != 'ok' or old_gen > 157286400"
CRITICAL: ok: heap 298MB of 1GB (29.1%), 42 threads, 123 collections|'127.0.0.1_gc_count'=123c '127.0.0.1_gc_time'=834ms '127.0.0.1_heap_usage'=29.1%;80 '127.0.0.1_heap_used'=312475648B '127.0.0.1_old_gen'=201326592;;157286400 '127.0.0.1_threads'=42
```

**A fractional attribute keeps its decimals in thresholds and perfdata (process CPU load, 0.0-1.0):**

```
check_jvm attribute=cpu=java.lang:type=OperatingSystem/ProcessCpuLoad "warning=cpu > 0.5"
WARNING: ok: heap 298MB of 1GB (29.1%), 42 threads, 123 collections|'127.0.0.1_cpu'=0.72999;0.5 '127.0.0.1_gc_count'=123c '127.0.0.1_gc_time'=834ms '127.0.0.1_heap_usage'=29.1%;;90 '127.0.0.1_heap_used'=312475648B '127.0.0.1_threads'=42
```

**An MBean the agent cannot find is CRITICAL and named in the message; the unread attribute is not graphed:**

```
check_jvm attribute=x=com.example:type=Cache/Hits
CRITICAL: error: x: com.example:type=Cache: javax.management.InstanceNotFoundException : com.example:type=Cache: heap 298MB of 1GB (29.1%), 42 threads, 123 collections|'127.0.0.1_gc_count'=123c '127.0.0.1_gc_time'=834ms '127.0.0.1_heap_usage'=29.1%;80;90 '127.0.0.1_heap_used'=312475648B '127.0.0.1_threads'=42
```

**An unreachable agent is UNKNOWN (exit status 3), with the error as the message and no performance data:**

```
check_jvm url=http://127.0.0.1:8778/jolokia/
http://127.0.0.1:8778/jolokia/: error: Failed to connect to 127.0.0.1:8778: Connection refused
```
