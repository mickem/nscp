`NSClientServer` implements the original NSClient protocol (TCP `12489` by default) spoken by the Nagios `check_nt`
plugin. It answers the classic check_nt variables (`CLIENTVERSION`, `CPULOAD`, `UPTIME`, `USEDDISKSPACE`,
`SERVICESTATE`, `PROCSTATE`, `MEMUSE`, `COUNTER`, `FILEAGE`, `INSTANCES`) by mapping them onto the modern checks
(`check_cpu`, `check_uptime`, `check_drivesize`, ...), so the corresponding modules (`CheckSystem`, `CheckDisk`) must
also be loaded for those variables to work.

<!-- @formatter:off -->
!!! danger "check_nt is a legacy protocol - avoid it for new setups"
    The protocol predates modern transport security: the password travels in cleartext in every request and virtually
    no check_nt client can speak TLS. Prefer NRPE or the REST API and leave this module disabled unless you must
    support an existing check_nt deployment. See the
    [securing guide](../../setup/securing.md#check_nt-legacy-nsclient-protocol) for the full risk discussion and
    migration advice.
<!-- @formatter:on -->

To use this module you need to enable it:

```
[/modules]
NSClientServer = enabled
```

#### Securing the server

Three settings make up the security configuration; all live under `[/settings/NSClient/server]` (`allowed hosts` and
`password` can also be inherited from `[/settings/default]`):

*   **`password`** - required. A server without a password refuses every request, so this must be set for the module
    to answer at all. The client sends it with `check_nt -s <password>`. Since the protocol transmits it in cleartext
    on every request, use a password dedicated to check_nt and used nowhere else. A wrong or missing password is
    answered with a generic `ERROR: Bad request.` so the reply does not reveal whether the password or the request was
    the problem.
*   **`allowed hosts`** - restrict which addresses may connect at all. This is the strongest control the protocol
    offers; limit it to your monitoring server(s), e.g. `allowed hosts = 127.0.0.1,192.168.0.10`.
*   **`use ssl`** - defaults to `true`, but the standard nagios-plugins `check_nt` cannot speak TLS, so
    interoperating with it requires explicitly setting `use ssl = false`. Doing so is your consent to running the
    protocol in cleartext - treat the network path as untrusted and rely on `allowed hosts` (and network segmentation)
    instead.

#### Restricting commands with `allow`

By default the server answers every check_nt variable (`allow = any`). The `allow` setting caps that: it is a comma
separated list where each entry is a group, the keyword `any`/`all`, or an individual command name. Anything not
enabled is refused before dispatch with `ERROR: Command not allowed.`

| Token                | Enables                                          | Note                                              |
|----------------------|--------------------------------------------------|---------------------------------------------------|
| `any` / `all`        | everything                                       | full check_nt compatibility (**default**)         |
| `metrics`            | `cpuload`, `uptime`, `useddiskspace`, `memuse`   | harmless aggregate system metrics                 |
| `info`               | `clientversion`                                  |                                                   |
| `service`            | `servicestate`                                   | service enumeration                               |
| `process`            | `procstate`                                      | process enumeration                               |
| `counters`           | `counter`, `instances`                           | arbitrary performance-counter reads               |
| `files`              | `fileage`                                        | arbitrary file existence/age probing              |
| individual command   | just that command                                | e.g. `uptime`, `counter`, `servicestate`, ...     |

Unknown tokens are logged and enable nothing, and an `allow` list that resolves to no commands makes the server refuse
every request (fail closed) - so a typo narrows access rather than widening it.

A locked-down configuration that only exposes aggregate system metrics and the agent version - denying the
arbitrary-read commands (`counter`, `fileage`, `instances`) and the service/process enumeration - looks like this:

```
[/settings/NSClient/server]
allow = metrics, info
allowed hosts = 192.168.0.10
password = a-password-used-only-for-check_nt
use ssl = false
```
