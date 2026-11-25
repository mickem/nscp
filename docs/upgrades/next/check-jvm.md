---
icon: "☕"
modules: [CheckNet]
action: none
---
**New check: `check_jvm` (Java applications over JMX via Jolokia).** Java
applications running as services — Tomcat, Keycloak, WildFly, ERP middleware —
are opaque to process-level checks. `check_jvm` reads their JMX MBeans through
a [Jolokia](https://jolokia.org/) agent (HTTP+JSON, no Java on the monitoring
side) in a single bulk POST: heap (`heap_used`, `heap_max`, `heap_usage`),
non-heap, threads (`threads`, `daemon_threads`, `peak_threads`), garbage
collection summed over all collectors (`gc_count`, `gc_time`), loaded classes,
uptime and the VM name/version. Any other MBean attribute — active sessions,
connection-pool usage, a cache hit count — becomes a keyword of its own with
`attribute=alias=mbean/attribute[/path]`; a numeric value keeps its
decimals in thresholds and perfdata, and an attribute that could not be read
renders as `unread` and satisfies no numeric comparison rather than reading
as 0. The defaults warn at
`heap_usage > 80` and go critical at `heap_usage > 90` or when the agent
answers with an error or any requested MBean cannot be read. An agent that
cannot be reached at all (connection refused, timeout) makes the check
UNKNOWN, with no performance data. The default URL is the Jolokia JVM agent's
`http://127.0.0.1:8778/jolokia/`; Basic authentication and TLS take the same
options as `check_tomcat_status`. The check is marked experimental.
