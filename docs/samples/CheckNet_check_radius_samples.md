#### Successful PAP authentication

Captured against the local UDP fixture, with `secret.txt` and `password.txt`
containing its test credentials. The port and elapsed time vary between runs.

```text
nscp client --module CheckNet --boot --query check_radius host=127.0.0.1 port=49576 secret-file=secret.txt username=test-user password-file=password.txt
OK: 127.0.0.1:49576 ok, reply=access_accept, time=1ms|'127.0.0.1_49576_time'=1ms;0;0
```

#### Responsiveness through an expected rejection

Captured with the fixture returning an authenticated Access-Reject:

```text
nscp client --module CheckNet --boot --query check_radius host=127.0.0.1 port=49576 secret-file=secret.txt mode=reject
OK: 127.0.0.1:49576 ok, reply=access_reject, time=0ms|'127.0.0.1_49576_time'=0ms;0;0
```

#### Latency and address-family selection

Configuration example for a registered production probe identity:

```text
check_radius host=nps.example.net secret-file=C:\monitoring\radius-secret.txt username=nps-monitor password-file=C:\monitoring\radius-password.txt address-family=ipv4 "warning=time > 500" timeout=3000
```

Use `mode=status` only with a server that supports Status-Server and returns both
authenticators. `port=1813` selects the usual accounting endpoint for that mode;
it does not send Accounting-Start/Stop records.
