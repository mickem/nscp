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
| `time`       | Round-trip time in milliseconds (perf data; `?` when the host never resolved) |

Defaults: **WARNING** when `time > 1000`, **CRITICAL** when `responding = 0`.

A host whose name does not resolve never starts an exchange, so it has no
round-trip time to report: `time` renders as `?` and contributes no perf data
rather than putting a sentinel into the series. It still goes **CRITICAL** on
`responding = 0`.

Options: `server=<host>` (repeatable) picks the KDC(s) to probe and
`realm=<REALM>` the realm; both default to what the domain join discovers
(`DsGetDcName`). A discovered realm is uppercased the way Active Directory
reports it; an explicit `realm=` is sent exactly as typed, since Kerberos
realms are case sensitive and a non-AD KDC may serve a lowercase one (max 255
characters). On a machine that is not domain-joined, `server=` and `realm=` are
required and the check says so with **UNKNOWN**. `timeout=<ms>` (default 5000)
bounds the probes; all KDCs are probed concurrently, so it also bounds the
whole check even when several KDCs are unreachable.
