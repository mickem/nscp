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

Keywords (a single row):

| Keyword         | Description                                                        |
|-----------------|--------------------------------------------------------------------|
| `domain`        | The trusted domain the secure channel points at                    |
| `dc`            | The domain controller the channel is established with              |
| `healthy`       | True when the channel is established (and verified)                |
| `error_code`    | Win32 status of the channel (0 = healthy)                          |
| `error_message` | `OK` or the formatted failure message                              |

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
