# Passive Monitoring with NSCA-NG

**Goal:** Have NSClient++ run checks on a schedule and push results to an
**NSCA-NG** server using TLS-PSK authentication — the modern, encrypted
replacement for the legacy NSCA daemon.

!!! tip "NSCA vs. NSCA-NG vs. NRDP"
    All three solve the same problem (push results from agent → server). Pick
    by what your monitoring server supports:

    - **NSCA** — original protocol, custom block cipher, widely deployed.
      [Scenario](passive-monitoring-nsca.md).
    - **NSCA-NG** — the same role over TLS with PSK or X.509. Stronger
      security, simpler text protocol. *(this page)*
    - **NRDP** — HTTP/JSON-based, easiest to traverse proxies and load
      balancers.
      [Scenario](passive-monitoring-nsca.md#using-nrdp-instead-of-nsca).

---

## How NSCA-NG Works

The agent opens a TLS connection to the server (default port `5668`),
authenticates with a Pre-Shared Key (PSK), then exchanges a small text
protocol:

```
Client ──TLS handshake (PSK)──► Server
Client → MOIN 1 <session-id>
Server → OKAY
Client → PUSH <length>
Server → OKAY
Client → [<ts>] PROCESS_SERVICE_CHECK_RESULT;<host>;<svc>;<code>;<output>\n
Server → OKAY
Client → QUIT
Server → OKAY
```

Each PUSH carries one Nagios external command. The server appends it to its
configured command file (Nagios/Icinga's `external_command_pipe`) and the
monitoring core picks it up like any other passive check.

---

## Prerequisites

```ini
[/modules]
CheckSystem  = enabled   ; provides check_cpu, check_memory, etc.
CheckDisk    = enabled   ; provides check_drivesize
Scheduler    = enabled   ; runs checks on a timer
NSCANgClient = enabled   ; pushes results via NSCA-NG
```

You also need an NSCA-NG server reachable on port `5668` (default), with a
matching PSK identity + password configured. A typical server snippet
(`nsca-ng.cfg`):

```text
listen   = "*:5668"
command  = "/usr/local/nagios/var/rw/nagios.cmd"

authorize "agent01" {
    password = "shared-secret-do-not-use-in-production"
}
```

---

## Step 1 — Verify the Connection

Before wiring up the scheduler, push a single test result from the agent:

```
nscp nsca-ng --host <server> --password "shared-secret-do-not-use-in-production" \
    --identity "agent01" \
    --hostname "agent01" \
    --command "test_service" \
    --result 0 --message "hello from NSClient++"
```

Expected output:

```
Submission successful
```

If you get a TLS handshake failure, the most common causes are (in order):

- Wrong **password** — the agent's `password` and the server's `password` for
  the matching `authorize` block must be byte-identical.
- Wrong **identity** — the agent's `--identity` must match an `authorize`
  block on the server. By default the identity is the agent's hostname.
- The server only supports TLS 1.3 PSK — NSClient++ uses the legacy
  `SSL_CTX_set_psk_client_callback` API, which requires TLS 1.2. NSClient++
  forces TLS 1.2 automatically when `use psk = true`; if the server is
  configured for TLS 1.3-only PSK, switch the server (or configure the
  agent for cert-based TLS instead, see below).

---

## Step 2 — Configure a Target

```ini
[/settings/NSCA-NG/client/targets/default]
address          = nsca-ng.example.com
port             = 5668
identity         = agent01
password         = shared-secret-do-not-use-in-production
timeout          = 30          ; per-operation timeout (connect/read/write)
retries          = 2           ; total attempts = retries + 1
host check       = false       ; submit as service check by default
max output length = 65536      ; clamp on plugin output (bytes)
```

`address`, `identity`, and `password` are mandatory. Everything else has
sensible defaults; `port` defaults to 5668, `timeout` to 30s,
`retries` to 2, `host check` to false, `max output length` to 64 KiB.

### Cert-based TLS instead of PSK

If your server uses X.509 certificates rather than PSK:

```ini
[/settings/NSCA-NG/client/targets/default]
address         = nsca-ng.example.com
use psk         = false
ca              = ${certificate-path}/ca.pem
certificate     = ${certificate-path}/agent.pem
certificate key = ${certificate-path}/agent.key
verify mode     = peer-cert
```

When `use psk = false` the connection uses TLS 1.2 or 1.3 with the standard
verify chain — no PSK ciphersuite restriction.

!!! warning "Cert mode now fails closed without peer verification"
    `verify mode` (combined with a valid `ca`) **must** authenticate the server
    when running in cert mode. If the resolved verify mode does not include
    `peer` (for example, the setting is missing, or set to `none`), the agent
    refuses to connect with:

    ```
    Refusing to connect: TLS peer verification is disabled and PSK is not in
    use. Either configure 'verify mode = peer-cert' with a 'ca = <path>',
    re-enable 'use psk = true', or set 'insecure = true' to override.
    ```

    The hostname in `address` must also match the server certificate's CN /
    SAN; if you connect to an IP literal, the cert needs that IP listed as a
    SAN.

---

## Step 3 — Configure the Scheduler

Same as the legacy NSCA scenario, but point the channel at `NSCA-NG`:

```ini
[/settings/scheduler/schedules/default]
channel  = NSCA-NG
interval = 5m
report   = all

[/settings/scheduler/schedules]
host_check = check_ok
cpu        = check_cpu
memory     = check_memory
disk_c     = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
```

For cron-style schedules, real-time channels (CheckEventLog / CheckLogFile),
and per-schedule overrides see
[Passive Monitoring → Step 2 — Configure the Scheduler](passive-monitoring-nsca.md#step-2-configure-the-scheduler).
The scheduler machinery is shared; only the `channel` differs.

---

## Step 4 — Host Checks vs. Service Checks

NSClient++ submits service checks by default. Two ways to send a host check:

1. **Per target:** set `host check = true` on the target. Every result
   submitted on that target becomes a `PROCESS_HOST_CHECK_RESULT`.
2. **Per schedule (legacy convention):** name the schedule `host_check` —
   the result is then automatically submitted as a host check, regardless of
   the target's `host check` setting:
   ```ini
   [/settings/scheduler/schedules]
   host_check = check_ok
   ```

The `host_check` alias was the original way and is preserved for
backwards compatibility.

---

## Step 5 — Restart and Verify

```
net stop nscp
net start nscp
```

If the server logs received commands, you should see entries like:

```
[1714973400] PROCESS_SERVICE_CHECK_RESULT;agent01;cpu;0;OK: CPU load is ok.|...
```

Set NSClient++'s log level to `trace` to see the full client side, including
the MOIN/PUSH/QUIT exchange:

```ini
[/settings/log]
level = trace
```

---

## Common Gotchas

### TLS handshake hangs

The agent applies the `timeout` setting to *every* network operation
(connect, handshake, read, write). If the handshake hangs longer than
`timeout` seconds the submission fails with `TLS handshake timed out` and
moves on to the retry, if any. Bump `timeout` if your network is slow; lower
it if you want faster failover to a different target.

### "FAIL bad password"

The PSK password and identity must match an `authorize` block on the
server exactly. Whitespace, case, and trailing newlines all matter.

### Output truncated

The `max output length` setting (default 64 KiB) clamps how many bytes of
plugin output are forwarded. Long stack traces or eventlog dumps can hit
this — bump the value, or summarise on the agent before sending.

### Server-side rejection is **not** retried

Network/TLS failures retry with backoff. Server-side responses (`FAIL`,
`BAIL`) are reported back to the caller without retrying — there's no point
hammering a server that's actively refusing the credentials.

### "Refusing to connect: TLS peer verification is disabled..."

You're running in cert mode (`use psk = false`) without a verifying chain.
Either set `verify mode = peer-cert` and supply a working `ca`, switch back
to PSK with `use psk = true`, or — only if you understand the MITM risk —
set `insecure = true` to permit unauthenticated TLS.

---

## Troubleshooting Checklist

When a submission fails silently from the monitoring server's point of view,
check these in order:

1. **Agent log at trace level** — does it show `Submission successful` or an
   error?
2. **Agent log for retry messages** — `NSCA-NG attempt N/M failed: ...` lines
   indicate transient network failures.
3. **Server log** — most NSCA-NG servers log every accepted command and every
   PSK rejection.
4. **Wireshark on port 5668** — TLS records should be visible; if not, the
   agent isn't reaching the server (firewall / DNS / port).
5. **`nscp nsca-ng --host ...`** — bypass the scheduler and send a one-off
   submission to isolate scheduler config from network/auth config.

---

## Next Steps

- [Passive Monitoring (NSCA/NRDP)](passive-monitoring-nsca.md) — the legacy
  NSCA / HTTP-based NRDP variants.
- [Passive Monitoring (Icinga 2)](passive-monitoring-icinga.md) — Icinga 2's
  REST API as a third option.
- [Reference: NSCANgClient](../reference/client/NSCANgClient.md) — every
  setting in detail.
