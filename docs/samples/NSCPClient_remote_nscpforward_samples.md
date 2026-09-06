**Invoking the command:**

The name matches none of the dispatch prefixes or suffixes the client framework
recognises (`forward_*`, `*_forward`, `check_*`, `*_query`, `exec_*`,
`submit_*`), so the call never reaches the relay code:

```
remote_nscpforward host=10.0.2.50 port=8443 command=check_ok
remote_nscpforward not found
```

The same answer comes back regardless of the arguments, and regardless of
whether anything is listening at the other end.

**Relaying NSCP traffic today:**

Configure the module's `fallback` handler on the target instead. A request this
agent does not handle itself is then passed to the configured NSCP target and
the answer returned unchanged, which is the behaviour this command was meant to
expose:

```ini
[/modules]
NSCPClient = enabled

[/settings/NSCP/client/targets/default]
address = nscp://10.0.2.50:8443
password = <shared secret>
verify mode = peer
ca = /etc/nsclient/ca.pem
```

From the monitoring server the relay is then invisible — it addresses the relay
and gets the far agent's result, with status, message and performance data
intact, because NSCP carries the request and response as structured data.

**Forwarding a named check explicitly:**

```
check_remote_nscp target=relay command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used|'C:\ used'=91.2GB;80;90;0;100 'C:\ used %'=91%;80;90;0;100
```

**Two consequences of relaying at all:**

A relay that does not inspect requests asks the far end for whatever the caller
asked for, so restrict what may be forwarded on the relay itself. And because it
terminates one connection and opens another, the far end sees the *relay* as the
client — any password or certificate-based authorisation there applies to the
relay, not to the original caller.
