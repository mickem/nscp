---
icon: "🔒"
modules: [WEBServer]
action: none
---
**Repeated failed WEB logins from one IP are now blocked for longer each
time.** The block that follows `auth rate limit max failures` consecutive
failures used to be a fixed `auth rate limit block seconds` window (default
60 s) that let an attacker resume guessing at a steady rate forever. Each
further block from the same IP now doubles the wait, up to an hour, and resets
after a successful authentication or an hour of quiet. Nothing to do on a
default install — a mistyped password still costs only the base delay. A
health check that deliberately probes with bad credentials will be blocked for
longer; `auth rate limit max failures = 0` still disables the limiter outright.
See the
[security notice](../security/notices.md#failed-web-authentication-now-backs-off-exponentially).
