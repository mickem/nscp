#### About `check_jvm`

`check_jvm` reads a Java application's JMX MBeans through a
[Jolokia](https://jolokia.org/) agent, which bridges JMX to HTTP+JSON. Nothing
Java-specific is needed on the NSClient++ side: the check sends one bulk
`POST` to the agent and reads, in a single round trip,

| MBean                                    | Keywords                                                    |
|------------------------------------------|-------------------------------------------------------------|
| `java.lang:type=Memory`                  | `heap_used`, `heap_committed`, `heap_max`, `heap_usage`, `heap`, `nonheap_used`, `nonheap_committed` |
| `java.lang:type=Threading`               | `threads`, `daemon_threads`, `peak_threads`                 |
| `java.lang:type=Runtime`                 | `uptime`, `vm_name`, `vm_vendor`, `vm_version`, `spec_version` |
| `java.lang:type=ClassLoading`            | `loaded_classes`                                            |
| `java.lang:type=GarbageCollector,name=*` | `gc_count`, `gc_time`, `collectors` (summed over every collector) |

plus any attributes requested with `attribute=`.

#### Installing the agent

The Jolokia JVM agent is a single jar attached at JVM start-up:

```
java -javaagent:/opt/jolokia/jolokia-agent-jvm-javaagent.jar=port=8778,host=127.0.0.1 -jar app.jar
```

For a Tomcat installed as a Windows service, add the same `-javaagent:...`
line to the *Java Options* of the service (`tomcat10w.exe //ES//Tomcat10`).
Alternatively deploy the Jolokia WAR agent into the servlet container and point
`url=` at it (`http://127.0.0.1:8080/jolokia/`). Keep the agent bound to
`127.0.0.1` when NSClient++ runs on the same host, or protect it with
authentication (`username=` / `password=`) and TLS otherwise: JMX can do far
more than read metrics.

#### Thresholds

By default the check goes **warning** above 80% heap usage and **critical**
above 90%, or when the agent cannot be reached or any requested read fails
(`result != 'ok'`). `heap_usage` is relative to the maximum heap (`-Xmx`);
for a JVM without a defined maximum it falls back to the committed heap.
`-w` / `-c` set the `heap_usage` bounds directly (`w=75 c=90`).

`heap_used`, `heap_max` and the other byte keywords take size units
(`heap_used > 2G`), `uptime` and `gc_time` take durations (`uptime < 10m`,
`gc_time > 5m`). `gc_count` and `gc_time` are totals since the JVM started,
so they suit a graph better than a threshold.

#### Application MBeans

`attribute=alias=mbean/attribute[/inner/path]` reads one more attribute and
exposes it as the keyword `alias` (and as perfdata). Numeric values compare
as numbers — write them as plain numbers, size units only work on the
built-in byte keywords — and anything else compares as a string; a composite
value without an inner path is rendered as its JSON. A `/` inside the MBean
name is written `!/` (Jolokia's own escape), so Tomcat's
`Catalina:type=Manager,host=localhost,context=/app` becomes
`context=!/app`. The option can be repeated.

A read that Jolokia cannot answer (a missing MBean, an access-policy denial)
sets `result` to `error: <alias>: <mbean>: <message>` while the other values
are still reported.
