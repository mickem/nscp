---
icon: "🔒"
modules: [WEBServer, NCPAServer]
action: conditional
---
**The bundled Mongoose web server is upgraded to 7.24, and a request with
more than 30 headers is now refused.** This concerns the **Windows builds**
of the `WEBServer` and `NCPAServer` modules; the Linux and macOS packages
use the Boost.Beast backend and do not contain Mongoose. Mongoose parses
request headers into a table of 30. Up to 7.23 a request carrying more was
served with the surplus silently ignored; from 7.24 it does not parse, and
the connection is closed without an answer (the log gets an
`HTTP parse, <n> bytes` line at info level). Browsers and monitoring clients send
far fewer, so most installs see no difference. **Check** setups where
NSClient++ sits behind a reverse proxy, load balancer or WAF that adds many
headers of its own (`X-Forwarded-*`, tracing, client-certificate details): if
requests through it start failing with a dropped connection, trim the headers
the proxy forwards. The rest of the upstream release fixes components
NSClient++ does not use (the built-in TCP/IP and TLS stacks, WebSocket, MQTT,
directory listing). See the
[security notice](../security/notices.md#mongoose-web-backend-headers-past-the-30th-were-silently-ignored).
