`nrpe_forward` is not normally invoked by hand: it re-sends a request that has
already arrived at this agent, so it is configured as a fallback on a target and
then used implicitly.

**Configure this agent as an NRPE relay:**

```ini
[/modules]
NRPEServer = enabled
NRPEClient = enabled

[/settings/NRPE/client/targets/default]
address = nrpe://10.0.2.50:5666
verify = peer
ca = /etc/nsclient/ca.pem

[/settings/NRPE/client]
channel = NRPE
```

With `nrpe_forward` registered as the fallback, a request the relay does not
handle itself is passed on to `10.0.2.50` and the answer returned unchanged.

**From the monitoring server, the relay is invisible:**

```
check_nrpe --host 192.168.56.10 --command check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

The server addressed the relay; the result came from the agent behind it.

**It takes no options of its own:**

The relay branch in the client framework hands the incoming request straight to
the handler without building or parsing an option descriptor, so `host=`,
`port=` and `command=` are **not** interpreted here — the destination comes only
from the target configuration, and any arguments given are appended to the
request that goes out on the wire. There is nothing useful to invoke by hand;
configure the target and let requests arrive.

**Two consequences of forwarding "as-is":**

The relay does not inspect the request, so whatever the caller asks for is what
the far end is asked to run — restrict what may be forwarded on the relay
itself. And because it terminates one TLS connection and opens another, the far
end sees the *relay* as the client, so any certificate-based authorisation there
applies to the relay rather than to the original caller.

**Nothing listening on the far end:**

The failure surfaces at the monitoring server as if the check itself had failed
— the relay reports the connection error from its own attempt to reach the
configured target:

```
check_nrpe --host 192.168.56.10 --command check_drivesize
UNKNOWN: Error: Failed to connect to: 10.0.2.50:5666 :Connection refused
```
