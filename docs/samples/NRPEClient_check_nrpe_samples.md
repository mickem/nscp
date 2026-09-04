The examples below run against an NSClient++ agent whose NRPE server is
listening on `127.0.0.1:15666` in legacy insecure (anonymous-DH) mode.

**Run a check on the remote host:**

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_ok
OK: No message
```

**A real check, with its performance data:**

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.41848GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
```

**Pass arguments to the remote check (`argument=`, repeatable):**

The remote agent must be configured with `allow arguments = true`, or the
arguments are refused there.

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_ok "argument=message=hello from NRPE"
OK: hello from NRPE
```

**Use a configured target instead of spelling out the connection:**

Put the host, port and TLS material under `[/settings/NRPE/client/targets/...]`
and the command line stays short — and the credentials stay out of process
listings.

```
check_nrpe target=web01 command=check_drivesize
OK: OK All 3 drive(s) are ok
```

**Run several checks over one connection:**

```
check_nrpe host=127.0.0.1 port=15666 insecure=true "batch=check_ok|message=first" "batch=check_drivesize"
OK: first
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```

**A command the remote agent does not know:**

The error comes from the far end, not from the client.

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_no_such_thing
UNKNOWN: Unknown command(s): check_no_such_thing
```

**Nothing listening:**

```
check_nrpe host=127.0.0.1 port=15667 insecure=true command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15667 :Connection refused
```

**Client and server disagreeing about TLS:**

Dropping `insecure=true` against a server running in insecure mode fails the
handshake rather than falling back. Both ends must be configured the same way.

```
check_nrpe host=127.0.0.1 port=15666 command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15666 :sslv3 alert handshake failure (SSL routines)
```

`insecure=true` means anonymous Diffie-Hellman: encrypted, but with neither end
authenticated. Where both ends are NSClient++, configure real certificates and
`verify=peer` instead.

**Truncated output on protocol version 2:**

Version 2 has a fixed 1024-byte payload. If a check's output is cut off at a
suspiciously round length, set `version=3` (or raise `payload length=` to match
the remote daemon exactly — a mismatch corrupts the exchange rather than
reporting an error).

```
check_nrpe host=192.168.56.103 command=check_files "argument=path=/var/log" version=3
OK: 412 files found
```
