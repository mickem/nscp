**Check a Tomcat server via the manager status page (the `?XML=true` parameter is appended automatically):**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret
OK: http-nio-8080 ok: 4/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_thread_usage'=2;75;90 'http-nio-8080_threads_busy'=4;0;0 'ajp-nio-8009_thread_usage'=0;75;90 'ajp-nio-8009_threads_busy'=0;0;0
```

**The default thresholds alert when a connector's thread pool fills up (75%/90%):**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret
WARNING: http-nio-8080 ok: 160/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_thread_usage'=80;75;90 'http-nio-8080_threads_busy'=160;0;0 'ajp-nio-8009_thread_usage'=0;75;90 'ajp-nio-8009_threads_busy'=0;0;0
```

**Alert on request errors per connector:**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret "warning=error_count > 10"
WARNING: http-nio-8080 ok: 4/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_error_count'=17c;10;0 'http-nio-8080_thread_usage'=2;0;90 'http-nio-8080_threads_busy'=4;0;0 'ajp-nio-8009_error_count'=0c;10;0 'ajp-nio-8009_thread_usage'=0;0;90 'ajp-nio-8009_threads_busy'=0;0;0
```

**Alert on a shrinking JVM heap:**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret "critical=memory_free < 100000000"
OK: http-nio-8080 ok: 4/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_memory_free'=1734127416B;0;100000000 ...
```

**Missing or wrong manager credentials are CRITICAL:**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status
CRITICAL:  http_401: 0/0 threads busy|'_thread_usage'=0%;75;90 '_threads_busy'=0;0;0
```
