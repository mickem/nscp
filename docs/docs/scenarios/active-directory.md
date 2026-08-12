# Active Directory & Identity Monitoring

**Goal:** Monitor the Windows identity stack end to end: domain controller replication,
the machine-account secure channel on every domain member, Kerberos KDC availability,
and — via performance counters — the NTDS directory service, AD Certificate Services
and AD FS.

Domain controllers are the most business-critical Windows role there is: when AD is
degraded, "everything is broken" for users while per-service port checks stay green.
The checks below catch the failure modes that matter, in rough order of blast radius.

---

## Prerequisites

Enable the `CheckActiveDirectory` module (and `CheckSystem` for the counter-based
checks) in `nsclient.ini`:

```ini
[/modules]
CheckActiveDirectory = enabled
CheckSystem          = enabled   ; check_pdh for NTDS / ADCS / ADFS counters
```

All checks in this guide are Windows-only.

---

## Domain controller replication

Replication failures are the classic silent AD killer: a DC that has not replicated
for longer than the tombstone lifetime is permanently orphaned and must be rebuilt.
Run this on **every** domain controller (replication state is per-DC):

```
check_ad_replication
```

### Expected output (healthy)

```
OK: all 6 replication links are healthy|'DC02 DC=example,DC=com'=0;0;4 ...
```

### Alert output

```
CRITICAL: DC02 DC=example,DC=com: 7 failures, last success 2026-08-10 03:11:42|...
```

Defaults: WARNING on the first failed sync (`consecutive_failures > 0`), CRITICAL
after five in a row or 24 hours without a successful sync. On a non-DC the check
returns UNKNOWN ("Not a domain controller"), so it is safe to deploy fleet-wide.

### Customisation

```
# Only alert on prolonged outages, ignore single hiccups:
check_ad_replication "warning=none" "critical=last_success < -24h"

# Check a remote DC:
check_ad_replication server=dc02.example.com
```

---

## Machine-account secure channel (every domain member)

A broken secure channel ("the trust relationship between this workstation and the
primary domain failed") blocks every domain logon on that host. The check actively
verifies the channel, like `Test-ComputerSecureChannel`:

```
check_secure_channel
```

### Expected output

```
OK: secure channel to EXAMPLE via DC01.example.com: OK
```

Workgroup machines return UNKNOWN ("not joined to a domain"), so the same check can
go to the whole fleet. Use `verify=false` for a passive status query that does not
contact the DC.

---

## Kerberos KDC availability

`check_kdc` sends a real (unauthenticated) Kerberos `AS-REQ` to the KDC and expects
a Kerberos answer — typically `KDC_ERR_PREAUTH_REQUIRED`, which is the *healthy*
response. A plain port check cannot see a KDC that accepts connections but no longer
issues tickets; this can.

```
check_kdc
```

### Expected output

```
OK: dc01.example.com: KRB-ERROR KDC_ERR_PREAUTH_REQUIRED (2ms)|'dc01.example.com'=2ms;1000
```

On a domain-joined machine the KDC and realm are discovered automatically; from
anywhere else, name them explicitly — no domain membership, account or Kerberos
configuration is needed:

```
check_kdc server=dc01.example.com server=dc02.example.com realm=EXAMPLE.COM
```

Defaults: WARNING when the round trip exceeds 1 second, CRITICAL when a KDC does not
answer with a well-formed Kerberos message.

---

## NTDS directory service counters (check_pdh)

The directory service exposes rich health counters under the `NTDS` object. Predefine
the interesting ones in `nsclient.ini` so checks are shell-quoting-free and can be
averaged over time (see the [PDH scenario](counters.md) for the mechanics):

```ini
[/settings/system/windows/counters/ad_repl_queue]
collection strategy = rrd
counter             = \NTDS\DRA Pending Replication Synchronizations

[/settings/system/windows/counters/ad_ldap_bind_time]
collection strategy = rrd
counter             = \NTDS\LDAP Bind Time

[/settings/system/windows/counters/ad_ldap_sessions]
collection strategy = rrd
counter             = \NTDS\LDAP Client Sessions

[/settings/system/windows/counters/ad_ldap_searches]
collection strategy = rrd
counter             = \NTDS\LDAP Searches/sec
```

Then alert on averages instead of instantaneous spikes:

```
# Replication backlog building up:
check_pdh counter=ad_repl_queue time=5m "warn=value > 50" "crit=value > 500"

# LDAP binds getting slow (milliseconds):
check_pdh counter=ad_ldap_bind_time time=5m "warn=value > 30" "crit=value > 100"

# Session count for capacity trending (no thresholds, perf data only):
check_pdh counter=ad_ldap_sessions time=5m
```

Other counters worth knowing: `\NTDS\DRA Inbound Bytes Total/sec`,
`\NTDS\DRA Outbound Bytes Total/sec` (replication volume), `\NTDS\DS Directory
Reads/sec`, `\NTDS\DS Directory Writes/sec` (directory load), and
`\NTDS\Kerberos Authentications/sec` / `\NTDS\NTLM Authentications/sec` (an NTLM
uptick often means Kerberos trouble).

---

## AD Certificate Services (check_pdh)

A stalled enterprise CA silently breaks certificate auto-enrolment for the whole
estate. The `Certification Authority` counter object (one instance per CA) makes it
visible; `check_certificate` (CheckSecurity) covers the CA's own certificate expiry.

```ini
[/settings/system/windows/counters/ca_failed_requests]
collection strategy = rrd
counter             = \Certification Authority(*)\Failed Requests/sec

[/settings/system/windows/counters/ca_request_time]
collection strategy = rrd
counter             = \Certification Authority(*)\Request processing time (ms)
```

```
# Any failed request is worth a look, a stream of them is an incident:
check_pdh counter=ca_failed_requests time=5m "warn=value > 0" "crit=value > 1"

# Issuance latency:
check_pdh counter=ca_request_time time=5m "warn=value > 500" "crit=value > 2000"
```

The instance wildcard `(*)` covers the (normally single) CA instance; use
`nscp sys -- --expand-path "\Certification Authority(*)\Requests/sec"` to see yours.

---

## AD FS token issuance (check_pdh)

ADFS outages break SSO to Microsoft 365 and every federated SaaS app. The `AD FS`
counter object exposes issuance rates and failures:

```ini
[/settings/system/windows/counters/adfs_token_requests]
collection strategy = rrd
counter             = \AD FS\Token Requests/sec

[/settings/system/windows/counters/adfs_federation_requests]
collection strategy = rrd
counter             = \AD FS\Federation Metadata Requests/sec
```

```
# Token issuance flatlining at 0 during business hours usually means broken SSO:
check_pdh counter=adfs_token_requests time=15m

# Pair with a service check on the ADFS service itself:
check_service service=adfssrv
```

On WAP/proxy nodes the counter set is `AD FS Proxy`. Counter availability varies by
ADFS version — validate with `nscp sys -- --validate "AD FS" --all` before wiring
alerts.

---

## Suggested per-role check sets

| Role                    | Checks                                                                                       |
|-------------------------|----------------------------------------------------------------------------------------------|
| Every domain member     | `check_secure_channel`                                                                       |
| Every domain controller | `check_ad_replication`, `check_kdc`, `check_service service=ntds service=netlogon service=kdc service=dns`, NTDS counters |
| Enterprise CA           | `Certification Authority` counters, `check_certificate` on the CA certificate               |
| ADFS farm               | `AD FS` counters, `check_service service=adfssrv`, `check_certificate` on the token-signing certificate |

---

## Next Steps

- [Performance Counter (PDH) Monitoring](counters.md) — predefined counters, averaging, localisation gotchas
- [Host Security Posture](security-posture.md) — `check_certificate` for CA / token-signing certificate expiry
- [Service & Process Monitoring](service-monitoring.md) — keep `ntds`, `netlogon`, `kdc`, `adfssrv` running
- [Reference: CheckActiveDirectory](../reference/check/CheckActiveDirectory.md) — full command reference
