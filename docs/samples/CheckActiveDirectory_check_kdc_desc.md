#### About `check_kdc`

`check_kdc` verifies that a Kerberos KDC is actually issuing responses — not
just that port 88 is open. It sends a real (unauthenticated) `AS-REQ` over TCP
and classifies the answer: an `AS-REP` or any `KRB-ERROR` (typically
`KDC_ERR_PREAUTH_REQUIRED`) proves a live KDC, while silence, a reset or a
non-Kerberos answer means authentication is down even though a port probe
would still pass. Kerberos failure looks like "everything is broken" to users,
so this is the check to point at every domain controller.

The probe uses a throwaway principal (`nscp-probe`) and never completes
authentication: no account, no password and no Kerberos configuration is
needed on the monitoring side.

Keywords (one row per probed KDC):

| Keyword      | Description                                                          |
|--------------|----------------------------------------------------------------------|
| `kdc`        | The host that was probed                                             |
| `realm`      | The Kerberos realm the probe requested a ticket for                  |
| `port`       | TCP port probed (default 88)                                         |
| `responding` | True when a well-formed Kerberos answer arrived                      |
| `response`   | What came back (`KRB-ERROR ...`, `AS-REP ...`, or the transport error) |
| `error_code` | KRB-ERROR code from the response (-1 when none)                      |
| `time`       | Round-trip time in milliseconds (perf data)                          |

Defaults: **WARNING** when `time > 1000`, **CRITICAL** when `responding = 0`.

Options: `server=<host>` (repeatable) picks the KDC(s) to probe and
`realm=<REALM>` the realm; both default to what the domain join discovers
(`DsGetDcName`). On a machine that is not domain-joined, `server=` and
`realm=` are required and the check says so with **UNKNOWN**. `timeout=<s>`
(default 5) bounds each probe.
