**Submit a passive result to an NSCA daemon:**

```
submit_nsca host=192.168.56.10 port=5667 command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**Submit several results at once:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_nsca host=192.168.56.10 "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**The usual arrangement — route results rather than calling this by hand:**

`NSCA` is the default channel for
[`check_and_forward`](../check/CheckHelpers.md#check_and_forward):

```ini
[/settings/NSCA/client/targets/default]
address = nsca://192.168.56.10:5667
encryption = aes
password = <shared secret>
payload length = 512
```

```
check_and_forward command=check_drivesize channel=NSCA alias=drivesize
OK: Message submitted: NSCA
```

**The three settings that must match the daemon exactly:**

`encryption` must equal the daemon's `decryption_method`, `password` its
`password`, and `payload length` its compiled-in `MAX_PACKETSIZE` (512 by
default). NSCA negotiates nothing, so a mismatch shows up as a dropped or
garbled result rather than an error — check these three first when results never
appear on the server.

**When the requested cipher is not in this build:**

Crypto++ is an optional dependency, so a package built without it offers only a
reduced set. The error names what is actually available:

```
submit_nsca host=192.168.56.10 command=nightly_backup result=CRITICAL "message=backup failed"
UNKNOWN: NSCA error: Unknown encryption algorithm: 'aes' (available: none = No Encryption (not safe), xor = XOR; use 'none' to disable encryption)
```

**When the far end is not an NSCA daemon:**

NSCA expects the server to open the exchange with an init packet, so a plain
listener on the port fails the handshake rather than accepting the result:

```
submit_nsca host=127.0.0.1 port=5667 command=nightly_backup result=CRITICAL "message=backup failed" encryption=none
UNKNOWN: Error: Retry failed
```

**A note on "encryption":**

The NSCA ciphers are shared-secret obfuscation over plain TCP — no server
authentication, no replay protection, no integrity. Wrap the connection in TLS
(`certificate`, `certificate key`, `ca`, `dh`, `allowed ciphers`) where the
receiving side supports it, or use [NSCA-ng](NSCANgClient.md) instead.
