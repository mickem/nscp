#### About `check_w32time`

`check_w32time` reports what the Windows Time service (W32Time) itself thinks:
whether the machine is following a time source at all, which source that is, how
far the clock was last computed to be off and which peers are configured. This
is the inside-out counterpart to CheckNet's `check_ntp_offset`, which probes an
NTP server from the outside: a domain member whose time hierarchy has broken
keeps answering with a plausible clock for hours while Kerberos ticket
validation is already on its way to failing, and only the service's own view
shows it.

The data is assembled from four places:

| Source | What it gives |
|---|---|
| Service control manager | Whether W32Time exists, is running, and how it starts. |
| `HKLM\SYSTEM\CurrentControlSet\Services\W32Time\Parameters` | `Type` (the synchronization mode) and `NtpServer` (the configured peers). |
| `HKLM\SYSTEM\CurrentControlSet\Services\W32Time\Config\LastKnownGoodTime` | When the service last recorded the clock as good. |
| `W32TimeQuerySource` (w32time.dll) | The source the running service is actually following. Like `w32tm /query /source`, this needs privilege: the agent has it running as a service, an unprivileged caller gets access denied and the check falls back to the configured peers. |
| "Windows Time Service" PDH counters | Computed time offset, NTP round trip delay, clock frequency adjustment and the number of time sources in use. |

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `service_state` | string | `running`, `stopped`, `starting`, … or `not installed`. |
| `start_type` | string | `auto`, `delayed`, `demand`, `disabled`, … |
| `sync_type` | string | Configured mode: `NT5DS` (domain hierarchy), `NTP`, `AllSync`, `NoSync`. |
| `source` | string | The time source in use, or the configured peers — see `source_from`. |
| `source_from` | string | `service` (asked the running service), `configuration` or `unknown`. |
| `peers` | string | Configured NTP peers, comma separated. Empty on a domain member, which discovers its source instead of being given one. |
| `last_sync` | string | Time of the last synchronization W32Time recorded as good, or `unknown`. |
| `state` | string | One-line verdict; what the default output shows. |
| `installed` | bool | W32Time exists on this host. |
| `running` | bool | The service is running. |
| `synchronized` | bool | The machine is following a time source — see the evidence order below. |
| `local_clock` | bool | The source is the machine's own clock (`Local CMOS Clock`, `Free-running System Clock`). |
| `peer_count` | int | Number of configured peers. |
| `offset` | int | Absolute clock offset against the source, milliseconds (perfdata). |
| `delay` | int | NTP round trip delay to the source, milliseconds (perfdata). |
| `frequency_adjustment` | int | Correction applied to the clock frequency, parts per billion; negative slows the clock down (perfdata). |
| `time_sources` | int | Number of NTP time sources in use. |
| `last_sync_age` | int | Seconds since the last known good synchronization (perfdata). |

The last five come from counters the service only maintains while it runs. When
there is no measurement they render as `unknown`, compare false against every
number (so a threshold like `offset > 1000` cannot fire on a missing value) and
emit no perfdata. Test for the absence explicitly with `offset = 'unknown'`.

`synchronized` ranks its evidence rather than guessing. The service not running
or `Type=NoSync` settles it on its own. Otherwise, when the service could be
asked what it follows, that answer decides — the local clock means
unsynchronized, anything else means synchronized. When it could not be asked,
`time_sources = 0` (no time source in use) decides instead. With neither piece
of evidence the check reports the configured intent and does not raise an alarm,
so a host where the counters are unavailable does not alert forever.

Default thresholds: **critical** when `synchronized = 0 or offset > 30000` and
**warning** when `offset > 1000`. The critical is the important one — it fires
when the machine follows no time source at all, whether because the service is
not running, because `Type` is `NoSync`, or because it has fallen back to its
own clock. Kerberos rejects tickets once the clock is five minutes out, so the
30-second critical leaves room to act.

On a **workgroup** machine Windows trigger-starts W32Time and stops it again
between synchronizations, so `running` is 0 most of the time and the default
critical fires by design. Check the configuration and the age of the last good
synchronization there instead, e.g.
`check_w32time "critical=sync_type = 'NoSync'" "warning=last_sync_age > 604800"`.
On a server or domain member the service is expected to run continuously and the
defaults apply as they are. **Windows only.**
