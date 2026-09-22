---
icon: "🔒 💥"
modules: [NSCPClient, CheckMKClient, CheckNet]
action: conditional
---
**The NSCP and check_mk clients, and `check_nsclient_web_online`, now verify
the server certificate by default.** All three encrypted their connection but
never authenticated the peer: `verify mode` defaulted to `none` on an
`NSCPClient` target, was left unset on a `CheckMKClient` target (an empty
verify mode parses the same as `none`), and `check_nsclient_web_online`
defaulted `verify` to `none`. `verify mode` / `verify` now defaults to `peer`
and `ca` to the agent's own trust bundle (`${ca-path}` — the auto-generated
ROOT store export on Windows, the distribution bundle elsewhere).

Because an NSClient++ agent generates a **self-signed** certificate on first
start, a relay or REST check pointed at a default agent will now fail the
handshake where it used to connect. Pick one per target:

| Situation | Configuration |
|---|---|
| The remote agent uses a certificate from your own CA | `ca = <the CA>` (the default `verify mode = peer` then works) |
| The remote agent still uses its self-signed certificate | `ca = <that certificate>` and `verify mode = peer-cert` |
| You accept an unauthenticated link | `verify mode = none` (encrypted, but an on-path attacker can impersonate the remote) |

```ini
[/settings/NSCP/client/targets/web01]
address = nscp://192.168.56.103:8443
password = <the remote's admin password>
ca = /etc/nsclient/agent-web01.pem
verify mode = peer-cert
```

For `check_nsclient_web_online` the same choice is made per check with `ca=`
and `verify=`:

```
check_nsclient_web_online host=agent.example.com password=... \
    ca=/etc/nsclient/agent.pem verify=peer-cert
```

`CheckMKClient` targets are only affected when they enable TLS (`use ssl =
true`); a stock check_mk agent listens in plain text and is unchanged.

See the
[security notice](../security/notices.md#nscp-and-check_mk-clients-sent-credentials-over-unverified-tls).
