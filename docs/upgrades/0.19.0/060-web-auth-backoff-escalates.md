---
icon: "🔒"
modules: [WEBServer]
action: none
---
**Repeated rapid-fire failed WEB logins from one IP are now blocked for longer
each time.** The block that follows `auth rate limit max failures` consecutive
failures used to be a fixed `auth rate limit block seconds` window (default
60 s) that let an attacker resume guessing at a steady rate forever. A further
block now doubles the wait, up to an hour, and resets after a successful
authentication or an hour of quiet. Only failures arriving at machine speed
escalate: a client retrying a stale password on a schedule keeps the base
delay, so it cannot lock out everyone sharing its address behind NAT or a
proxy. Nothing to do on a default install. `auth rate limit max failures = 0`
still disables the limiter outright. See the
[security notice](../security/notices.md#failed-web-authentication-now-backs-off-exponentially).
