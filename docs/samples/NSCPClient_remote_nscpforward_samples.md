`remote_nscpforward` is not normally invoked by hand: it re-sends a request that
has already arrived at this agent, so it is configured as a fallback on a target
and then used implicitly.

**Configure this agent as an NSCP relay:**

```ini
[/modules]
NSCPClient = enabled
WEBServer = enabled

[/settings/NSCP/client/targets/default]
address = nscp://10.0.2.50:8443
password = <shared secret>
verify mode = peer
ca = /etc/nsclient/ca.pem
```

With `remote_nscpforward` registered as the fallback, a request the relay does
not handle itself is passed on to `10.0.2.50` and the answer returned unchanged.

**From the monitoring server, the relay is invisible:**

```
check_remote_nscp target=relay command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used|'C:\ used'=91.2GB;80;90;0;100 'C:\ used %'=91%;80;90;0;100
```

The server addressed the relay; the result — status, message and performance
data — came from the agent behind it, unaltered. Because NSCP carries the
request and the response as structured data, the relay is lossless in a way an
NRPE relay is not.

**Invoking it explicitly (mainly useful for testing the hop):**

```
remote_nscpforward host=10.0.2.50 port=8443 command=check_ok
OK: No message
```

**Two consequences of forwarding "as-is":**

The relay does not inspect the request, so whatever the caller asks for is what
the far end is asked to run — restrict what may be forwarded on the relay
itself. And because it terminates one connection and opens another, the far end
sees the *relay* as the client, so any password or certificate-based
authorisation there applies to the relay rather than to the original caller.

**Nothing listening on the far end:**

```
remote_nscpforward host=10.0.2.50 port=8443 command=check_ok
UNKNOWN: Error: Failed to connect to: 10.0.2.50:8443 :Connection refused
```
