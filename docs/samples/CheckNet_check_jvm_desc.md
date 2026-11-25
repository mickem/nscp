#### About `check_jvm`

`check_jvm` reads a Java application's JMX MBeans through a
[Jolokia](https://jolokia.org/) agent, which bridges JMX to HTTP+JSON. Nothing
Java-specific is needed on the NSClient++ side: the check sends one bulk
`POST` to the agent and reads, in a single round trip, the JVM's platform
MBeans (`java.lang:type=Memory`, `Threading`, `Runtime`, `ClassLoading` and
every `GarbageCollector`, whose counts and times are summed) together with
any attributes requested with `attribute=`. The keywords they become are
listed under *Filter keywords* below.

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
(`result != 'ok'`).

!!! warning "Keep `result != 'ok'` when you set `critical`"
    A `critical=` expression replaces the default one, including its
    `result != 'ok'`. Leave that out and a crashed JVM or an unreachable agent
    no longer alerts: the values then have no value (`unknown`, or `unread`
    for an attribute), a custom threshold such as `old_gen > 157286400` is not
    met, and the check returns OK. Write your own condition next to it:
    `"critical=result != 'ok' or old_gen > 157286400"`.

`heap_usage` is relative to the maximum heap (`-Xmx`); for a JVM without a
defined maximum it falls back to the committed heap. `-w` / `-c` set the
`heap_usage` bounds directly (`w=75 c=90`).

When the agent cannot be reached, or Jolokia answers one of the platform
reads with an error, the keywords of that read have no value rather than 0:
they render as `unknown`, satisfy no numeric comparison and are left out of
the perfdata. A graph then shows a gap instead of an empty heap, and a
`gc_count` counter is not seen to drop to 0 (which a rate graph would read as
a JVM restart). The failed read itself sets `result`, which is what alerts.

`heap_used`, `heap_max` and the other byte keywords take size units
(`heap_used > 2G`), `uptime` and `gc_time` take durations (`uptime < 10m`,
`gc_time > 5m`). `gc_count` and `gc_time` are totals since the JVM started,
so they suit a graph better than a threshold.

#### Application MBeans

`attribute=alias=mbean/attribute[/inner/path]` reads one more attribute and
exposes it as the keyword `alias`. A numeric value compares as a number with
its decimals (`cpu > 0.5`) and becomes perfdata (`<host>_<alias>`); write the
threshold as a plain number, since size units only work on the built-in byte
keywords. Anything else compares as a string (`state = 'STARTED'`), and a
composite value without an inner path is rendered as its JSON.

An attribute that could not be read renders as `unread`, and one whose value
is not a number has no numeric value. Neither satisfies any numeric
comparison - `sessions < 1` does not fire on a missing MBean - and neither is
graphed, so a gap in the data never shows up as a 0. The failed read itself
still sets `result` (below), which is what alerts on it.

The alias must start with a letter and use only letters, digits and
underscores. The words of the filter language (`and`, `or`, `not`, `in`,
`like`, `regexp`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `str`), names starting
with `not` followed by a letter or digit, the keywords `check_jvm` already has
and the generic summary names (`count`, `list`, `lines`, `status`, ...) are
refused.

A `/` inside the MBean name is written `!/` (Jolokia's own escape), so
Tomcat's `Catalina:type=Manager,host=localhost,context=/app` becomes
`context=!/app`. The option can be repeated.

A read that Jolokia cannot answer (a missing MBean, an access-policy denial)
sets `result` to `error: <alias>: <mbean>: <message>` while the other values
are still reported.
