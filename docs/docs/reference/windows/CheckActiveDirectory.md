# CheckActiveDirectory

*Available on Windows only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

CheckActiveDirectory checks Active Directory health: replication on domain controllers, the machine-account secure channel and Kerberos KDC availability.

## Enable module

To enable this module and allow using the commands you need to add `CheckActiveDirectory = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckActiveDirectory = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckActiveDirectory module.

**List of commands:**

A list of all available queries (check commands)

| Command                                                        | Description                                                                                                                 |
|----------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| [check_ad_replication](#check_ad_replication) *(experimental)* | Check inbound Active Directory replication links on a domain controller (last success, consecutive failures). Windows only. |
| [check_kdc](#check_kdc) *(experimental)*                       | Check that Kerberos KDCs answer an AS-REQ probe on port 88 (a real Kerberos exchange, not just a port check). Windows only. |
| [check_secure_channel](#check_secure_channel) *(experimental)* | Verify the machine-account secure channel to the domain via netlogon. Windows only.                                         |

### check_ad_replication

Check inbound Active Directory replication links on a domain controller (last success, consecutive failures). Windows only.

#### About `check_ad_replication`

`check_ad_replication` reads the inbound replication state of a domain
controller straight from the directory service (`DsReplicaGetInfo`, the same
source `repadmin /showrepl` uses). Each inbound replication link — a (naming
context, source DC) pair — becomes one row: when it last attempted and last
managed to sync, and how many attempts in a row have failed.

Replication failures are the classic silent AD killer: a DC that has not
replicated for longer than the tombstone lifetime (typically 60–180 days) is
permanently orphaned and must be rebuilt. This check alerts long before that.

Defaults: **WARNING** when `consecutive_failures > 0`, **CRITICAL** when
`consecutive_failures > 4 or last_success < -24h`. A link that has *never*
synced trips the 24-hour rule by design.

Options: `server=<dc>` checks another domain controller (default: the local
machine — replication state is per-DC, so run the check on every DC).
`timeout=<ms>` (default 5000) bounds the whole read, the local machine
included.

**Not-a-DC contract:** on a host that is not a domain controller, the check
returns **UNKNOWN** with a "Not a domain controller" message rather than a hard
error, so it is safe to deploy fleet-wide. The machine role is what decides
this (`DsRoleGetPrimaryDomainInformation`), not the bind failure itself: a real
domain controller that fails to answer — stopped NTDS, access denied, RPC
unavailable — is reported as a plain failure, because that is the outage this
check exists to surface. A single-DC domain (no replication partners) returns
**OK** with an explanatory empty-state message.

**Timeout:** none of the directory service calls take a timeout of their own,
so the check runs the read on a worker thread and stops waiting for it when
`timeout=` runs out, reporting **UNKNOWN** ("No answer from the directory
service on dc02 within 5000ms"). That covers the case a port check misses: a
firewall that lets the RPC endpoint mapper (TCP 135) through but drops the
dynamic RPC port the directory service answers on, where the bind would
otherwise block for as long as RPC keeps retrying. A remote `server=` is still
probed on port 135 first, so a host that is down or blocked outright reports
exactly that. Windows cannot cancel the blocked call, so the worker is left to
finish on its own; until it has, further runs against the same server report
that the previous read has not returned instead of starting another thread.

**Jump to section:**

* [Sample Commands](#check_ad_replication_samples)
* [Command-line Arguments](#check_ad_replication_options)
* [Filter keywords](#check_ad_replication_filter_keys)


<a id="check_ad_replication_samples"></a>
#### Sample Commands

**Default check (healthy domain controller):**

```
check_ad_replication
OK: all 6 replication links are healthy|'DC02 DC=example,DC=com'=0;0;4 'DC02 CN=Configuration,DC=example,DC=com'=0;0;4 ...
```

**A partner has been failing for a while:**

```
check_ad_replication
CRITICAL: DC02 DC=example,DC=com: 7 failures, last success 2026-08-10 03:11:42|'DC02 DC=example,DC=com'=7;0;4 ...
```

**Only alert on prolonged outages (ignore single hiccups):**

```
check_ad_replication "warning=none" "critical=last_success < -24h"
OK: all 6 replication links are healthy
```

**Check a remote domain controller:**

```
check_ad_replication server=dc02.example.com
OK: all 6 replication links are healthy
```

**Custom output listing every link and its last error:**

```
check_ad_replication "top-syntax=${status}: ${list}" "detail-syntax=${source} -> ${naming_context}: ${last_error_message}"
WARNING: DC02 -> DC=example,DC=com: The RPC server is unavailable., DC03 -> DC=example,DC=com: 
```

**On a host that is not a domain controller (the fleet-wide-safe contract):**

```
check_ad_replication
Not a domain controller: Failed to bind to the directory service on WEB01: 6d9: There are no more endpoints available from the endpoint mapper.
```

**On the only domain controller of a single-DC domain:**

```
check_ad_replication
No replication partners found (single domain controller?)
```



<a id="check_ad_replication_options"></a>
#### Command-line Arguments

<a id="check_ad_replication_server"></a>

        
        
| Option                                   | Default Value | Description                                                                      |
|------------------------------------------|---------------|----------------------------------------------------------------------------------|
| server                                   |               | The domain controller to check (default: the local machine).                     |
| [timeout](#check_ad_replication_timeout) | 5000          | Timeout in milliseconds for the whole read, the directory service bind included. |



<h5 id="check_ad_replication_timeout">timeout:</h5>

Timeout in milliseconds for the whole read, the directory service bind included.

*Default Value:* `5000`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                                                                               |
|----------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
| <a id="check_ad_replication_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                             |
| <a id="check_ad_replication_warning"></a>[warning](../common-options.md#warning)                                     | consecutive_failures > 0                                                                    |
| <a id="check_ad_replication_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                             |
| <a id="check_ad_replication_critical"></a>[critical](../common-options.md#critical)                                  | consecutive_failures > 4 or last_success < -24h                                             |
| <a id="check_ad_replication_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                             |
| <a id="check_ad_replication_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                             |
| <a id="check_ad_replication_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                       |
| <a id="check_ad_replication_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                       |
| <a id="check_ad_replication_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                                                          |
| <a id="check_ad_replication_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                             |
| <a id="check_ad_replication_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                       |
| <a id="check_ad_replication_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                           |
| <a id="check_ad_replication_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                                                                  |
| <a id="check_ad_replication_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): all %(count) replication links are healthy                                       |
| <a id="check_ad_replication_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No replication partners found (single domain controller?)                                   |
| <a id="check_ad_replication_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${source} ${naming_context}: ${consecutive_failures} failures, last success ${last_success} |
| <a id="check_ad_replication_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${source} ${naming_context}                                                                 |
| <a id="check_ad_replication_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                             |
| <a id="check_ad_replication_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                             |
| <a id="check_ad_replication_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                          |
| <a id="check_ad_replication_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                             |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_ad_replication_filter_keys"></a>
#### Filter keywords

| Option               | Description                                                     |
|----------------------|-----------------------------------------------------------------|
| consecutive_failures | Number of consecutive failed sync attempts on this link         |
| failed               | True when the last sync attempt failed                          |
| last_attempt         | When the last sync was attempted                                |
| last_error           | Win32 result code of the last sync attempt (0 = success)        |
| last_error_message   | Human readable message for the last sync result (empty when ok) |
| last_success         | When the last sync succeeded (epoch 0 = never)                  |
| naming_context       | The replicated directory partition (naming context) DN          |
| source               | The source domain controller this link replicates from          |
| source_address       | Transport address of the source (GUID-based DNS name)           |
| source_dsa           | Full DN of the source directory service agent                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_kdc

Check that Kerberos KDCs answer an AS-REQ probe on port 88 (a real Kerberos exchange, not just a port check). Windows only.

#### About `check_kdc`

`check_kdc` verifies that a Kerberos KDC is actually issuing responses — not
just that port 88 is open. It sends a real (unauthenticated) `AS-REQ` over TCP
and classifies the answer: an `AS-REP` or any `KRB-ERROR` proves a live KDC,
while silence, a reset or a non-Kerberos answer means authentication is down
even though a port probe would still pass. Kerberos failure looks like
"everything is broken" to users, so this is the check to point at every domain
controller.

#### Which account the probe names

The `AS-REQ` names an account that exists: by default this machine's own
(`HOST$`, the account Windows itself authenticates with), which is available
whenever the probed realm is the domain the machine is joined to. The KDC
answers it with `KDC_ERR_PREAUTH_REQUIRED` — the *healthy* result — because the
probe never sends a password: it asks for a ticket, and the KDC asks it to
prove who it is. No password is ever sent and nothing counts against the
account's lockout.

A probe for a name that does not exist would get `KDC_ERR_C_PRINCIPAL_UNKNOWN`
instead. That still proves the KDC is alive, but the DC logs it as a failed
ticket request (event 4768), and a monitor doing that against every DC every
few minutes is exactly the pattern user-enumeration detections alert on. So
the check never invents one: to probe a realm this machine has no account in
(a foreign realm, or from a machine that is not domain-joined), name one with
`principal=`. Pick an account that exists and requires Kerberos
pre-authentication (every account does unless *Do not require Kerberos
preauthentication* is set on it) and is enabled: a disabled or expired account
is refused with its own logged failure. Without `principal=` the check returns
**UNKNOWN** saying so, before anything is sent.

The `principal` keyword shows which account was named.

#### Thresholds and options

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
characters). On a machine that is not domain-joined, `server=`, `realm=` and
`principal=` are required and the check says so with **UNKNOWN**.

`timeout=<ms>` (default 5000) bounds the whole probe, name lookups included:
all KDCs are looked up and probed concurrently under one deadline, so it also
bounds the whole check when several KDCs are unreachable. A lookup the DNS
server never answers is reported as `resolve failed: no answer from DNS in
time` when the deadline passes; Windows cannot cancel it, so it finishes on
its own in the background, and until it has, further runs against the same
host report that the previous lookup has not returned rather than starting
another.

**Jump to section:**

* [Sample Commands](#check_kdc_samples)
* [Command-line Arguments](#check_kdc_options)
* [Filter keywords](#check_kdc_filter_keys)


<a id="check_kdc_samples"></a>
#### Sample Commands

**Default check (domain-joined; probes the discovered KDC):**

```
check_kdc
OK: dc01.example.com: KRB-ERROR KDC_ERR_PREAUTH_REQUIRED (2ms)|'dc01.example.com'=2ms;1000
```

`KDC_ERR_PREAUTH_REQUIRED` is the *healthy* answer: the probe named this
machine's own account, and the KDC processed the request and asked it to
pre-authenticate.

**Probe specific KDCs explicitly (works from any machine, no domain join needed):**

```
check_kdc server=dc01.example.com server=dc02.example.com realm=EXAMPLE.COM principal=svc-monitor
OK: all 2 KDC(s) are responding|'dc01.example.com'=2ms;1000 'dc02.example.com'=3ms;1000
```

**A realm this machine has no account in, without `principal=`:**

```
check_kdc server=kdc.partner.test realm=PARTNER.TEST
principal= is required to probe PARTNER.TEST: this machine has no account in that realm. Name an account that exists there and requires pre-authentication.
```

**KDC down (nothing answering on the port):**

```
check_kdc server=dc01.example.com realm=EXAMPLE.COM principal=svc-monitor
CRITICAL: dc01.example.com: connect failed: No connection could be made because the target machine actively refused it (2028ms)|'dc01.example.com'=2028ms;1000
```

**Something answered, but it does not speak Kerberos:**

```
check_kdc server=dc01.example.com realm=EXAMPLE.COM principal=svc-monitor
CRITICAL: dc01.example.com: invalid response (5ms)|'dc01.example.com'=5ms;1000
```

**Tighten the latency alert (Kerberos slowness precedes logon storms):**

```
check_kdc "warning=time > 200" "critical=responding = 0 or time > 2000"
OK: dc01.example.com: KRB-ERROR KDC_ERR_PREAUTH_REQUIRED (2ms)|'dc01.example.com'=2ms;200;2000
```

**Custom output with the raw error code:**

```
check_kdc "detail-syntax=${kdc} port ${port} realm ${realm} as ${principal}: ${response} code=${error_code}"
OK: dc01.example.com port 88 realm EXAMPLE.COM as WS01$: KRB-ERROR KDC_ERR_PREAUTH_REQUIRED code=25
```

**On a machine that is not domain-joined (no server= given):**

```
check_kdc
Failed to locate a KDC (is this machine domain-joined?): 54b: The specified domain either does not exist or could not be contacted. Specify server=, realm= and principal=.
```



<a id="check_kdc_options"></a>
#### Command-line Arguments

<a id="check_kdc_server"></a>
<a id="check_kdc_realm"></a>
<a id="check_kdc_principal"></a>

        
        
        
        
        
| Option                        | Default Value | Description                                                                                                                                                                                                          |
|-------------------------------|---------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| server                        |               | KDC host to probe; can be given multiple times (default: the KDC located via the domain join).                                                                                                                       |
| realm                         |               | Kerberos realm to request a ticket for (default: the joined domain; required when not domain-joined).                                                                                                                |
| principal                     |               | Client principal to name in the AS-REQ (default: this machine's account, HOST$, when probing the domain it is joined to; required for any other realm). Name an account that exists and requires pre-authentication. |
| [port](#check_kdc_port)       | 88            | TCP port to probe.                                                                                                                                                                                                   |
| [timeout](#check_kdc_timeout) | 5000          | Timeout in milliseconds for the probes, name lookups included. All KDCs are probed concurrently, so this also bounds the whole check.                                                                                |



<h5 id="check_kdc_port">port:</h5>

TCP port to probe.

*Default Value:* `88`

<h5 id="check_kdc_timeout">timeout:</h5>

Timeout in milliseconds for the probes, name lookups included. All KDCs are probed concurrently, so this also bounds the whole check.

*Default Value:* `5000`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                    | Default Value                                 |
|-----------------------------------------------------------------------------------------------------------|-----------------------------------------------|
| <a id="check_kdc_filter"></a>[filter](../common-options.md#filter)                                        |                                               |
| <a id="check_kdc_warning"></a>[warning](../common-options.md#warning)                                     | time > 1000                                   |
| <a id="check_kdc_warn"></a>[warn](../common-options.md#warn)                                              |                                               |
| <a id="check_kdc_critical"></a>[critical](../common-options.md#critical)                                  | responding = 0                                |
| <a id="check_kdc_crit"></a>[crit](../common-options.md#crit)                                              |                                               |
| <a id="check_kdc_ok"></a>[ok](../common-options.md#ok)                                                    |                                               |
| <a id="check_kdc_debug"></a>[debug](../common-options.md#debug)                                           | false                                         |
| <a id="check_kdc_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                         |
| <a id="check_kdc_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                       |
| <a id="check_kdc_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                               |
| <a id="check_kdc_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                         |
| <a id="check_kdc_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                             |
| <a id="check_kdc_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                            |
| <a id="check_kdc_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): all %(count) KDC(s) are responding |
| <a id="check_kdc_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                               |
| <a id="check_kdc_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${kdc}: ${response} (${time}ms)               |
| <a id="check_kdc_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${kdc}                                        |
| <a id="check_kdc_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                               |
| <a id="check_kdc_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                               |
| <a id="check_kdc_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                            |
| <a id="check_kdc_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_kdc_filter_keys"></a>
#### Filter keywords

| Option     | Description                                                                               |
|------------|-------------------------------------------------------------------------------------------|
| error_code | KRB-ERROR code from the response (-1 when none)                                           |
| kdc        | The KDC host that was probed                                                              |
| port       | TCP port probed                                                                           |
| principal  | The client principal the probe named (this machine's account unless principal= was given) |
| realm      | The Kerberos realm the probe requested a ticket for                                       |
| responding | True when the KDC answered the AS-REQ with a well-formed Kerberos message                 |
| response   | What the KDC answered (or the transport error)                                            |
| time       | Probe round-trip time in milliseconds (none when the host never resolved)                 |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_secure_channel

Verify the machine-account secure channel to the domain via netlogon. Windows only.

#### About `check_secure_channel`

`check_secure_channel` verifies the machine-account secure channel — the
authenticated netlogon session every domain member maintains to a domain
controller. A broken secure channel ("the trust relationship between this
workstation and the primary domain failed") blocks every domain logon on the
host while port- and service-level checks keep reporting green, which makes it
one of the highest-signal single-bit checks a domain estate can run.

By default the check actively *verifies* the channel (netlogon `TC_VERIFY`,
the same operation as `nltest /sc_verify` / `Test-ComputerSecureChannel`),
which contacts the DC. Pass `verify=false` for a passive status query only.

Defaults: **CRITICAL** when `healthy = 0`; no warning threshold.

Options: `domain=<name>` checks the channel to a specific trusted domain
(default: the domain the checked machine is joined to); `server=<host>`
queries another computer's netlogon service (its join state is then also read
from that computer when `domain=` is not given).

**Not-joined contract:** on a workgroup or standalone machine the check
returns **UNKNOWN** ("not joined to a domain") rather than a hard error, so it
is safe to deploy fleet-wide.

**CRITICAL means a broken channel, nothing else:** when the netlogon query
itself fails — the service is stopped or restarting, the caller lacks
administrator rights, or the RPC connection to `server=` fails — the check
returns **UNKNOWN** with the failure message instead of scoring the channel as
broken. Verifying the channel requires administrator rights on the target,
which the NSClient++ service (LocalSystem) has; running the check as an
unprivileged user yields that UNKNOWN.

**Jump to section:**

* [Sample Commands](#check_secure_channel_samples)
* [Command-line Arguments](#check_secure_channel_options)
* [Filter keywords](#check_secure_channel_filter_keys)


<a id="check_secure_channel_samples"></a>
#### Sample Commands

**Default check (healthy domain member; actively verifies the channel):**

```
check_secure_channel
OK: secure channel to EXAMPLE via DC01.example.com: OK
```

**Broken secure channel (machine-account password out of sync):**

```
check_secure_channel
CRITICAL: secure channel to EXAMPLE via : The trust relationship between this workstation and the primary domain failed.
```

**Passive status query only (do not contact the DC):**

```
check_secure_channel verify=false
OK: secure channel to EXAMPLE via DC01.example.com: OK
```

**Check the channel to a specific trusted domain:**

```
check_secure_channel domain=PARTNER
OK: secure channel to PARTNER via DC05.partner.example: OK
```

**Custom output with the raw status code:**

```
check_secure_channel "detail-syntax=${domain}: dc=${dc} code=${error_code}"
OK: EXAMPLE: dc=DC01.example.com code=0
```

**On a workgroup machine (the fleet-wide-safe contract):**

```
check_secure_channel
This machine is not joined to a domain (workgroup WORKGROUP); there is no secure channel to check
```



<a id="check_secure_channel_options"></a>
#### Command-line Arguments

<a id="check_secure_channel_domain"></a>
<a id="check_secure_channel_server"></a>

        
        
        
| Option                                 | Default Value | Description                                                                                                              |
|----------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------|
| domain                                 |               | The trusted domain to check the channel to (default: the domain this machine is joined to).                              |
| server                                 |               | The computer whose secure channel to check (default: the local machine).                                                 |
| [verify](#check_secure_channel_verify) | true          | Actively verify the channel by contacting the DC (netlogon TC_VERIFY). Set verify=false for a passive status query only. |



<h5 id="check_secure_channel_verify">verify:</h5>

Actively verify the channel by contacting the DC (netlogon TC_VERIFY). Set verify=false for a passive status query only.

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                                           |
|----------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------|
| <a id="check_secure_channel_filter"></a>[filter](../common-options.md#filter)                                        |                                                         |
| <a id="check_secure_channel_warning"></a>[warning](../common-options.md#warning)                                     |                                                         |
| <a id="check_secure_channel_warn"></a>[warn](../common-options.md#warn)                                              |                                                         |
| <a id="check_secure_channel_critical"></a>[critical](../common-options.md#critical)                                  | healthy = 0                                             |
| <a id="check_secure_channel_crit"></a>[crit](../common-options.md#crit)                                              |                                                         |
| <a id="check_secure_channel_ok"></a>[ok](../common-options.md#ok)                                                    |                                                         |
| <a id="check_secure_channel_debug"></a>[debug](../common-options.md#debug)                                           | false                                                   |
| <a id="check_secure_channel_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                   |
| <a id="check_secure_channel_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                 |
| <a id="check_secure_channel_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                         |
| <a id="check_secure_channel_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                   |
| <a id="check_secure_channel_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                       |
| <a id="check_secure_channel_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                      |
| <a id="check_secure_channel_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                         |
| <a id="check_secure_channel_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                         |
| <a id="check_secure_channel_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | secure channel to ${domain} via ${dc}: ${error_message} |
| <a id="check_secure_channel_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${domain}                                               |
| <a id="check_secure_channel_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                         |
| <a id="check_secure_channel_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                         |
| <a id="check_secure_channel_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                      |
| <a id="check_secure_channel_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                         |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_secure_channel_filter_keys"></a>
#### Filter keywords

| Option        | Description                                                  |
|---------------|--------------------------------------------------------------|
| dc            | The domain controller the secure channel is established with |
| domain        | The trusted domain the secure channel points at              |
| error_code    | Win32 status of the secure channel (0 = healthy)             |
| error_message | Human readable channel state (OK or the failure message)     |
| healthy       | True when the secure channel is established and verified     |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

