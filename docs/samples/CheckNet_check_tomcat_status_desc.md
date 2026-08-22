#### About `check_tomcat_status`

`check_tomcat_status` fetches Apache Tomcat's manager status page in XML form
(`/manager/status?XML=true`) and reports one record per connector plus the JVM
heap numbers. The `?XML=true` parameter is appended automatically when the URL
does not already carry it.

The manager application must be deployed and the account used needs the
`manager-status` role (or `manager-gui`, which includes it) in
`conf/tomcat-users.xml`; pass it with `username=` / `password=`.

One record is emitted per connector (e.g. `http-nio-8080`, `ajp-nio-8009`).
By default the check goes **warning** at 75% and **critical** at 90% thread
pool usage, and **critical** when the page cannot be fetched or parsed
(`result != 'ok'` — including `http_401` for missing credentials).

Use `filter=connector like 'http'` to scope the check to specific connectors.
The JVM heap keywords repeat on every connector record, so combine them with a
`filter` to avoid the same memory alert firing once per connector.
