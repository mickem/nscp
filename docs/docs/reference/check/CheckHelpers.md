# CheckHelpers

Various helper function to extend other checks.

## Enable module

To enable this module and allow using the commands you need to add `CheckHelpers = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckHelpers = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckHelpers module.

**List of commands:**

A list of all available queries (check commands)

| Command                                         | Description                                                                         |
|-------------------------------------------------|-------------------------------------------------------------------------------------|
| [check_always_critical](#check_always_critical) | Run another check and regardless of its return code return CRITICAL.                |
| [check_always_ok](#check_always_ok)             | Run another check and regardless of its return code return OK.                      |
| [check_always_warning](#check_always_warning)   | Run another check and regardless of its return code return WARNING.                 |
| [check_and_forward](#check_and_forward)         | Run a check and forward the result as a passive check.                              |
| [check_critical](#check_critical)               | Just return CRITICAL (anything passed along will be used as a message).             |
| [check_multi](#check_multi)                     | Run more then one check and return the worst state.                                 |
| [check_negate](#check_negate)                   | Run a check and alter the return status codes according to arguments.               |
| [check_ok](#check_ok)                           | Just return OK (anything passed along will be used as a message).                   |
| [check_timeout](#check_timeout)                 | Run a check and timeout after a given amount of time if the check has not returned. |
| [check_version](#check_version)                 | Just return the NSClient++ version.                                                 |
| [check_warning](#check_warning)                 | Just return WARNING (anything passed along will be used as a message).              |
| [filter_perf](#filter_perf)                     | Run a check and filter performance data.                                            |
| [render_perf](#render_perf)                     | Run a check and render the performance data as output message.                      |
| [xform_perf](#xform_perf)                       | Run a check and transform the performance data in various (currently one) way.      |

**List of command aliases:**

A list of all short hand aliases for queries (check commands)

| Command             | Description                               |
|---------------------|-------------------------------------------|
| checkalwayscritical | Alias for: :query:`check_always_critical` |
| checkalwaysok       | Alias for: :query:`check_always_ok`       |
| checkalwayswarning  | Alias for: :query:`check_always_warning`  |
| checkcritical       | Alias for: :query:`check_critical`        |
| checkmultiple       | Alias for: :query:`check_multi`           |
| checkok             | Alias for: :query:`check_ok`              |
| checkversion        | Alias for: :query:`check_version`         |
| checkwarning        | Alias for: :query:`check_warning`         |
| negate              | Alias for: :query:`check_negate`          |
| timeout             | Alias for: :query:`check_timeout`         |

### check_always_critical

Run another check and regardless of its return code return CRITICAL.

#### About `check_always_critical`

`check_always_critical` runs another check and then **overwrites its status with
CRITICAL**, keeping the wrapped check's message and performance data intact.

The wrapped command and its arguments are passed positionally: everything after
the command name is handed to the wrapped check unchanged.

Use it to demote a check that is informational rather than actionable — you
still want its numbers graphed and its message in the service detail, but you do
not want it to page anyone. It is the blunt counterpart to
[`check_negate`](#check_negate), which remaps individual states rather than
collapsing all of them to one.

**The override is unconditional.** A misspelt command name, a module that is not
loaded, or a check that failed outright all come back as CRITICAL with the error
text as the message — the failure is visible only if somebody reads it. That
makes this a poor choice for anything you rely on to still be running; prefer
`check_negate` when you want to relabel some states but keep the ability to
notice that the check itself broke.

The legacy alias `CheckAlwaysCRITICAL` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_always_critical_samples)
* [Command-line Arguments](#check_always_critical_options)


<a id="check_always_critical_samples"></a>
#### Sample Commands

**Force a check's result to CRITICAL:**

The wrapped check's message survives; only the status is replaced.

```
check_always_critical check_critical "message=Nightly report generated"
CRITICAL: Nightly report generated
```

**With a real check:**

```
check_always_critical check_drivesize "crit=used > 50%"
CRITICAL: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
'/ used'=8.27045GB;201.57782;125.98613;0;251.97227 '/ used %'=3%;80;50;0;100 '/opt/claude-code used'=202.74609MB;183.95937;114.9746;0;229.94921 '/opt/claude-code used %'=88%;80;50;0;100
```

Performance data passes through unchanged, so the numbers are still graphed even
though the status has been overridden.

**A command that does not exist is also reported as CRITICAL:**

The override is unconditional, so a broken configuration hides behind it. Only
the message says what went wrong.

```
check_always_critical check_no_such_command
CRITICAL: Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_always_critical --arguments check_drivesize
CRITICAL: WARNING /opt/claude-code: 202.746MB/229.949MB used
```



<a id="check_always_critical_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_always_ok

Run another check and regardless of its return code return OK.

#### About `check_always_ok`

`check_always_ok` runs another check and then **overwrites its status with
OK**, keeping the wrapped check's message and performance data intact.

The wrapped command and its arguments are passed positionally: everything after
the command name is handed to the wrapped check unchanged.

Use it to demote a check that is informational rather than actionable — you
still want its numbers graphed and its message in the service detail, but you do
not want it to page anyone. It is the blunt counterpart to
[`check_negate`](#check_negate), which remaps individual states rather than
collapsing all of them to one.

**The override is unconditional.** A misspelt command name, a module that is not
loaded, or a check that failed outright all come back as OK with the error
text as the message — the failure is visible only if somebody reads it. That
makes this a poor choice for anything you rely on to still be running; prefer
`check_negate` when you want to relabel some states but keep the ability to
notice that the check itself broke.

The legacy alias `CheckAlwaysOK` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_always_ok_samples)
* [Command-line Arguments](#check_always_ok_options)


<a id="check_always_ok_samples"></a>
#### Sample Commands

**Force a check's result to OK:**

The wrapped check's message survives; only the status is replaced.

```
check_always_ok check_critical "message=Nightly report generated"
OK: Nightly report generated
```

**With a real check:**

```
check_always_ok check_drivesize "crit=used > 50%"
OK: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
'/ used'=8.27045GB;201.57782;125.98613;0;251.97227 '/ used %'=3%;80;50;0;100 '/opt/claude-code used'=202.74609MB;183.95937;114.9746;0;229.94921 '/opt/claude-code used %'=88%;80;50;0;100
```

Performance data passes through unchanged, so the numbers are still graphed even
though the status has been overridden.

**A command that does not exist is also reported as OK:**

The override is unconditional, so a broken configuration hides behind it. Only
the message says what went wrong.

```
check_always_ok check_no_such_command
OK: Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_always_ok --arguments check_drivesize
OK: WARNING /opt/claude-code: 202.746MB/229.949MB used
```



<a id="check_always_ok_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_always_warning

Run another check and regardless of its return code return WARNING.

#### About `check_always_warning`

`check_always_warning` runs another check and then **overwrites its status with
WARNING**, keeping the wrapped check's message and performance data intact.

The wrapped command and its arguments are passed positionally: everything after
the command name is handed to the wrapped check unchanged.

Use it to demote a check that is informational rather than actionable — you
still want its numbers graphed and its message in the service detail, but you do
not want it to page anyone. It is the blunt counterpart to
[`check_negate`](#check_negate), which remaps individual states rather than
collapsing all of them to one.

**The override is unconditional.** A misspelt command name, a module that is not
loaded, or a check that failed outright all come back as WARNING with the error
text as the message — the failure is visible only if somebody reads it. That
makes this a poor choice for anything you rely on to still be running; prefer
`check_negate` when you want to relabel some states but keep the ability to
notice that the check itself broke.

The legacy alias `CheckAlwaysWARNING` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_always_warning_samples)
* [Command-line Arguments](#check_always_warning_options)


<a id="check_always_warning_samples"></a>
#### Sample Commands

**Force a check's result to WARNING:**

The wrapped check's message survives; only the status is replaced.

```
check_always_warning check_critical "message=Nightly report generated"
WARNING: Nightly report generated
```

**With a real check:**

```
check_always_warning check_drivesize "crit=used > 50%"
WARNING: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
'/ used'=8.27045GB;201.57782;125.98613;0;251.97227 '/ used %'=3%;80;50;0;100 '/opt/claude-code used'=202.74609MB;183.95937;114.9746;0;229.94921 '/opt/claude-code used %'=88%;80;50;0;100
```

Performance data passes through unchanged, so the numbers are still graphed even
though the status has been overridden.

**A command that does not exist is also reported as WARNING:**

The override is unconditional, so a broken configuration hides behind it. Only
the message says what went wrong.

```
check_always_warning check_no_such_command
WARNING: Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_always_warning --arguments check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```



<a id="check_always_warning_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_and_forward

Run a check and forward the result as a passive check.

#### About `check_and_forward`

`check_and_forward` runs another check and submits its result as a **passive
check** on a channel — the same thing the Scheduler does when an
interval elapses, but on demand and for a single check.

Use it when you want a result to reach the monitoring server *now*: after
changing a check's arguments in `nsclient.ini`, while setting up a new NSCA or
NRDP target, or from a script that decides for itself when a result is
interesting.

The channel is the name a client module listens on — `NSCA` for the NSCA
client, `NRDP` for NRDP, `GRAPHITE`, `SYSLOG`, and so on (see the module's
`channel` setting). The client module then resolves the destination, encrypts
and sends the result exactly as it would for a scheduled check, so what arrives
on the server is indistinguishable from the scheduled version.

| Option        | What it is for                                                                                    |
|---------------|---------------------------------------------------------------------------------------------------|
| `command`     | The check to run. Required.                                                                       |
| `arguments`   | Arguments for that check, repeat for more than one. Not accepted positionally.                    |
| `channel`     | Channel to submit on, defaults to `NSCA`. `target` is kept as a legacy synonym.                   |
| `alias`       | Service description to report as, defaults to the name of the command.                            |
| `destination` | Which target of the client module to send to, defaults to that module's default target.           |
| `source`      | Source host name to report as, defaults to the host name of this machine.                         |

The command itself returns **OK** when the result was handed to the channel and
**UNKNOWN** when the check could not be run or the channel refused it (no such
channel, or the client module failed to send). It does **not** return the status
of the wrapped check — that status is what was submitted, and is visible on the
monitoring server.

!!! note

    The wrapped check runs with the permissions of whoever called
    `check_and_forward`, not with those of `CheckHelpers` — see
    [permissions](../../concepts/permissions.md). If the wrapped check is
    denied, nothing is submitted and the command returns the denial as an
    error.

!!! warning

    Before 0.18.0 the result never left the agent: the submission was built in a
    way the channels could not read, so the check ran, the command answered
    `Message submitted` and nothing at all reached the monitoring server. If you
    worked around this, the workaround is no longer needed.

**Jump to section:**

* [Sample Commands](#check_and_forward_samples)
* [Command-line Arguments](#check_and_forward_options)


<a id="check_and_forward_samples"></a>
#### Sample Commands

Given this configuration:

```ini
[/modules]
CheckSystem  = enabled
CheckHelpers = enabled
NSCAClient   = enabled

[/settings/NSCA/client/targets/default]
address    = nsca://nagios-server:5667
password   = secret-password
encryption = aes256
```

**Run a check and send its result as a passive check:**

```
nscp client --boot --query check_and_forward command=check_cpu channel=NSCA "alias=CPU Load"
Message submitted: NSCA
```

The NSCA daemon on the monitoring server logs the result under the service
description given by `alias`, exactly as a scheduled check would deliver it:

```
nsca: SERVICE CHECK -> Host Name: 'win-server-01', Service Description: 'CPU Load',
      Return Code: '0', Output: 'OK: CPU load is ok.|...'
```

**Pass arguments to the wrapped check** — one per `arguments=`:

```
nscp client --boot --query check_and_forward command=check_cpu channel=NSCA "alias=CPU Load" "arguments=warning=load>10"
Message submitted: NSCA
```

The status of the wrapped check is what gets submitted; the `OK` you see here
only says the result was handed to the NSCA client.

**The `-a` / `--argument` spelling works too**, which is what you want from a
batch file or a script:

```
nscp client --boot --query check_and_forward --argument command=check_cpu --argument channel=NSCA --argument "alias=CPU Load"
Message submitted: NSCA
```

**A channel nobody listens on is an error, not a silent drop:**

```
nscp client --boot --query check_and_forward command=check_cpu channel=NOPE
Failed to submit to: NOPE
```

**Over REST**, to push a result on demand from a script:

```
curl -k -u admin:<password> "https://<agent>:8443/api/v2/queries/check_and_forward/commands/execute?command=check_cpu&channel=NSCA&alias=CPU+Load"
{"command":"check_and_forward","result":0,"lines":[{"message":"Message submitted: NSCA","perf":{}}]}
```

#### Trying it without a monitoring server

`SimpleFileWriter` is a channel that writes results to a local file, which makes
it a quick way to see exactly what is being submitted:

```ini
[/modules]
CheckSystem      = enabled
CheckHelpers     = enabled
SimpleFileWriter = enabled

[/settings/writers/file]
file = results.txt
```

```
nscp client --boot --query check_and_forward command=check_cpu channel=FILE "alias=CPU Load"
Message submitted: FILE
```

`results.txt` then contains one line per submitted result, as
`${alias-or-command} ${result} ${message}`:

```
CPU Load OK OK: CPU load is ok.
```

Add an argument for the wrapped check and the forwarded status changes with it —
the check really runs, it is not a fixed OK:

```
nscp client --boot --query check_and_forward command=check_cpu channel=FILE "alias=CPU Load" "arguments=warning=load>-1"
Message submitted: FILE
```

```
CPU Load WARNING WARNING: 5s: 0%, 1m: 0%, 5m: 0%
```

Without `alias` the result is named after the command instead:

```
check_cpu OK OK: CPU load is ok.
```



<a id="check_and_forward_options"></a>
#### Command-line Arguments

<a id="check_and_forward_command"></a>
<a id="check_and_forward_arguments"></a>
<a id="check_and_forward_channel"></a>
<a id="check_and_forward_target"></a>
<a id="check_and_forward_alias"></a>
<a id="check_and_forward_destination"></a>
<a id="check_and_forward_source"></a>

| Option      | Default Value | Description                                                                                                          |
|-------------|---------------|----------------------------------------------------------------------------------------------------------------------|
| command     |               | The command to run before forwarding the result                                                                      |
| arguments   |               | An argument for the wrapped command, repeat for more than one                                                        |
| channel     |               | The channel to submit the result on, i.e. which client module sends it (NSCA, NRDP, GRAPHITE, ...). Defaults to NSCA |
| target      |               | Legacy synonym for channel (kept for backwards compatibility)                                                        |
| alias       |               | Alias (service description) to report the result as, defaults to the name of the command                             |
| destination |               | The target to send the message to (resolved by the client module, defaults to its default target)                    |
| source      |               | The name of the source system, defaults to the host name of this machine                                             |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_critical

Just return CRITICAL (anything passed along will be used as a message).

#### About `check_critical`

`check_critical` is a constant: it always returns **CRITICAL**, without running
anything. It exists so that a monitoring configuration can be exercised
end-to-end — that the transport works, that the command is allowed, that the
server renders the state — without depending on the health of the host.

The only option is `message=`, which sets the text returned alongside the
status. It defaults to `No message`.

The legacy alias `CheckCRITICAL` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_critical_samples)
* [Command-line Arguments](#check_critical_options)


<a id="check_critical_samples"></a>
#### Sample Commands

**Return CRITICAL with the default message:**

```
check_critical
CRITICAL: No message
```

**Return CRITICAL with your own message:**

```
check_critical "message=Service is down"
CRITICAL: Service is down
```

**As a placeholder for a check that is not implemented yet:**

Wiring a service to `check_critical` makes the gap visible on the dashboard
instead of leaving a silently missing check.

```
check_nrpe --host 192.168.56.103 --command check_critical --arguments "message=TODO: implement backup verification"
CRITICAL: TODO: implement backup verification
```



<a id="check_critical_options"></a>
#### Command-line Arguments

| Option                             | Default Value | Description       |
|------------------------------------|---------------|-------------------|
| [message](#check_critical_message) | No message    | Message to return |



<h5 id="check_critical_message">message:</h5>

Message to return

*Default Value:* `No message`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_multi

Run more then one check and return the worst state.

#### About `check_multi`

`check_multi` runs several checks in one round trip and returns the **worst**
status any of them produced, concatenating their messages and merging their
performance data.

Each check is given as one `command=` argument holding the whole command line —
the command name plus its arguments, quoted as a single token. `command=` may be
repeated; `arguments=` is a deprecated alias for it. `separator=` (default
`, `), `prefix=` and `suffix=` shape the combined message.

Status escalation follows the usual Nagios ordering — OK < WARNING < CRITICAL <
UNKNOWN — so one UNKNOWN check makes the whole result UNKNOWN. If any command
cannot be executed at all, the whole check fails rather than silently reporting
on the subset that ran.

Note that the checks run **sequentially**, so the total run time is the sum of
the parts; keep an eye on your monitoring system's check timeout when combining
several slow checks, and consider wrapping the slow one in
[`check_timeout`](#check_timeout).

The legacy alias `CheckMultiple` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_multi_samples)
* [Command-line Arguments](#check_multi_options)


<a id="check_multi_samples"></a>
#### Sample Commands

**Run two checks and return the worst status:**

Each check is one `command=` argument holding the whole command line. The result
carries one line per wrapped check.

```
check_multi "command=check_ok message=first" "command=check_warning message=second"
WARNING: first
WARNING: , second
```

**Shape the combined message with `separator` and `prefix`:**

```
check_multi "command=check_ok message=a" "command=check_ok message=b" "separator= | " "prefix=results: "
OK: results: a
OK:  | b
```

**Combine real checks into one service:**

```
check_multi "command=check_drivesize" "command=check_ok message=second"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.28236GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
WARNING: , second
```

The performance data of every wrapped check is merged into the result, so all
the numbers are still graphed against the one service.

**Status escalation:**

The worst status wins, in the order OK < WARNING < CRITICAL < UNKNOWN — so a
single check that returns UNKNOWN makes the whole result UNKNOWN even if the
others are merely CRITICAL.

```
check_multi "command=check_critical" "command=check_no_such_command"
UNKNOWN: No message
UNKNOWN: , Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_multi --arguments "command=check_drivesize" --arguments "command=check_uptime"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used, uptime: 5d 03:14h
```



<a id="check_multi_options"></a>
#### Command-line Arguments

<a id="check_multi_command"></a>
<a id="check_multi_arguments"></a>
<a id="check_multi_prefix"></a>
<a id="check_multi_suffix"></a>

| Option                              | Default Value | Description                                  |
|-------------------------------------|---------------|----------------------------------------------|
| command                             |               | Commands to run (can be used multiple times) |
| arguments                           |               | Deprecated alias for command                 |
| [separator](#check_multi_separator) | ,             | Separator between messages                   |
| prefix                              |               | Message prefix                               |
| suffix                              |               | Message suffix                               |



<h5 id="check_multi_separator">separator:</h5>

Separator between messages

*Default Value:* `, `


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_negate

Run a check and alter the return status codes according to arguments.

#### About `check_negate`

`check_negate` runs another check and **remaps its status**, leaving the message
and performance data untouched. It is the NSClient++ equivalent of the Nagios
`negate` plugin, and is available under the alias `negate`.

The command to run is named with `command=` (`-q`) and its arguments are passed
one per `arguments=` (`-a`). The four mapping options — `ok=` (`-o`),
`warning=` (`-w`), `critical=` (`-c`) and `unknown=` (`-u`) — each name the
state to return *instead of* that one. Every mapping defaults to itself, so the
options you omit pass through unchanged, and state names are parsed the usual
way (`ok`, `warning`, `critical`, `unknown`).

The classic use is inverting a check — "alert when this process *is* running",
"alert when this port *is* open".

**Beware that the mappings are applied in sequence to the value as it is being
rewritten, not to the original status.** The order is OK, WARNING, CRITICAL,
UNKNOWN, so a mapping that moves a status *forward* in that order can be picked
up and rewritten again by a later rule. `ok=critical critical=ok` is therefore
**not** a clean swap: an OK result is rewritten to CRITICAL by the first rule and
then straight back to OK by the third, so the OK half of the inversion silently
does nothing. The CRITICAL half works, because nothing after it rewrites OK.

The mappings that are safe are the ones that move a status *backwards*
(`critical=warning`, `unknown=critical`, `warning=ok`) and any single mapping
whose destination you do not also remap. To invert a check reliably, map only
the direction you actually need:

```
check_negate command=check_thing critical=ok
```

Unlike [`check_always_ok`](#check_always_ok) and its siblings, `check_negate`
keeps the distinctions between states; use it whenever you want to relabel
rather than flatten. If the wrapped command cannot be executed the check fails
outright and no mapping is applied.

**Jump to section:**

* [Sample Commands](#check_negate_samples)
* [Command-line Arguments](#check_negate_options)


<a id="check_negate_samples"></a>
#### Sample Commands

**Pass through unchanged (no mappings given):**

```
check_negate command=check_critical
CRITICAL: No message
```

**Invert a check — but only in one direction at a time:**

```
check_negate command=check_critical critical=ok
OK: No message
```

`ok=critical critical=ok` looks like a swap and is not one. The mappings are
applied in sequence to the value as it is rewritten, in the order OK, WARNING,
CRITICAL, UNKNOWN — so an OK result is rewritten to CRITICAL by the first rule
and back to OK by the third:

```
check_negate command=check_critical ok=critical critical=ok
OK: No message
```

Both report OK here only because the input was CRITICAL, which the third rule
maps to OK. An OK input takes the first rule to CRITICAL and then the third
straight back to OK, so the inversion never fires in that direction.

Map only the direction you need, and put it on the state the wrapped check
actually returns in the case you want to alert on.

**Downgrade CRITICAL to WARNING, leaving everything else alone:**

```
check_negate command=check_drivesize critical=warning
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```

**Pass arguments to the wrapped check:**

`arguments=` (`-a`) is repeatable; each one is handed to the wrapped command.

```
check_negate command=check_drivesize "arguments=crit=used > 50%" critical=warning
WARNING: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
```

**Treat UNKNOWN as CRITICAL:**

Useful where an UNKNOWN result — a missing counter, an unreachable service — is
just as actionable as a failure, and you do not want it filtered out by a
notification rule that ignores UNKNOWN.

```
check_negate command=check_no_such_command unknown=critical
CRITICAL: Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_negate --arguments "command=check_drivesize" --arguments "critical=warning"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```



<a id="check_negate_options"></a>
#### Command-line Arguments

<a id="check_negate_ok"></a>
<a id="check_negate_warning"></a>
<a id="check_negate_critical"></a>
<a id="check_negate_unknown"></a>
<a id="check_negate_command"></a>
<a id="check_negate_arguments"></a>

| Option    | Default Value | Description                             |
|-----------|---------------|-----------------------------------------|
| ok        |               | The state to return instead of OK       |
| warning   |               | The state to return instead of WARNING  |
| critical  |               | The state to return instead of CRITICAL |
| unknown   |               | The state to return instead of UNKNOWN  |
| command   |               | Wrapped command to execute              |
| arguments |               | List of arguments (for wrapped command) |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_ok

Just return OK (anything passed along will be used as a message).

#### About `check_ok`

`check_ok` is a constant: it always returns **OK**, without running
anything. It exists so that a monitoring configuration can be exercised
end-to-end — that the transport works, that the command is allowed, that the
server renders the state — without depending on the health of the host.

The only option is `message=`, which sets the text returned alongside the
status. It defaults to `No message`.

The legacy alias `CheckOK` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_ok_samples)
* [Command-line Arguments](#check_ok_options)


<a id="check_ok_samples"></a>
#### Sample Commands

**Return OK with the default message:**

```
check_ok
OK: No message
```

**Return OK with your own message:**

```
check_ok "message=Database backup completed"
OK: Database backup completed
```

**As a connectivity probe over NRPE:**

Nothing on the far end can make this fail, so a non-OK answer means the
transport, the permissions or the agent itself is the problem — not the host
being checked.

```
check_nrpe --host 192.168.56.103 --command check_ok
OK: No message
```



<a id="check_ok_options"></a>
#### Command-line Arguments

| Option                       | Default Value | Description       |
|------------------------------|---------------|-------------------|
| [message](#check_ok_message) | No message    | Message to return |



<h5 id="check_ok_message">message:</h5>

Message to return

*Default Value:* `No message`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_timeout

Run a check and timeout after a given amount of time if the check has not returned.

#### About `check_timeout`

`check_timeout` runs another check with a wall-clock deadline and returns
`Thread failed to return within given timeout` if the check has not answered in
time. It is available under the alias `timeout`.

The command to run is named with `command=` (`-q`) and its arguments are passed
one per `arguments=` (`-a`). `timeout=` (`-t`) is the deadline in seconds and
defaults to 30; `return=` (`-r`) overrides the status returned when the check
*does* finish in time.

Use it to stop a check that talks to something remote — a database, a share, an
HTTP endpoint — from hanging past your monitoring system's own timeout, which
would otherwise leave the service in a stale state with no message explaining
why.

Two things are worth knowing about how the deadline is enforced. On timeout the
worker thread is **detached, not killed**: the wrapped check keeps running to
completion in the background and only its result is discarded. A check that
hangs indefinitely therefore leaks a thread per invocation, so pair a short
timeout with a fix for whatever is hanging rather than treating it as a
permanent arrangement. And a timeout is reported as a *bad response* (UNKNOWN) —
`return=` only substitutes the status when the check finishes within the
deadline, so it cannot be used to make a timeout look like a success.

**Jump to section:**

* [Sample Commands](#check_timeout_samples)
* [Command-line Arguments](#check_timeout_options)


<a id="check_timeout_samples"></a>
#### Sample Commands

**Run a check with a deadline:**

A check that answers in time returns its own result unchanged.

```
check_timeout command=check_drivesize timeout=1
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.27112GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
```

**Pass arguments to the wrapped check:**

```
check_timeout command=check_drivesize timeout=10 "arguments=crit=used > 50%"
CRITICAL: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
```

**What a timeout looks like:**

Here `slow_thing` is an external script that sleeps for 30 seconds:

```
check_timeout command=slow_thing timeout=3
UNKNOWN: Thread failed to return within given timeout
```

The wrapped check is *detached*, not killed, so it goes on running in the
background — only its result is thrown away.

**Override the status of a check that does finish:**

`return=` applies only on success; it cannot turn a timeout into an OK.

```
check_timeout command=check_critical timeout=5 return=ok
OK: No message
```

**Guarding a slow remote check over NRPE:**

Set the timeout comfortably below the monitoring server's own check timeout so
you get a message explaining the hang rather than a bare plugin timeout.

```
check_nrpe --host 192.168.56.103 --command check_timeout --arguments "command=check_mssql" --arguments "timeout=20"
UNKNOWN: Thread failed to return within given timeout
```



<a id="check_timeout_options"></a>
#### Command-line Arguments

<a id="check_timeout_timeout"></a>
<a id="check_timeout_command"></a>
<a id="check_timeout_arguments"></a>
<a id="check_timeout_return"></a>

| Option    | Default Value | Description                             |
|-----------|---------------|-----------------------------------------|
| timeout   |               | The timeout value                       |
| command   |               | Wrapped command to execute              |
| arguments |               | List of arguments (for wrapped command) |
| return    |               | The return status                       |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_version

Just return the NSClient++ version.

#### About `check_version`

`check_version` returns the running NSClient++ version string as an OK result.
It takes no options and never fails, which makes it the cheapest possible
"is the agent alive and answering?" probe — useful as a heartbeat check, and as
the first thing to run when verifying a new NRPE/REST connection.

It reports only the version *string*. If you want to threshold on the version —
"alert when this fleet member falls behind" — use
[`check_nscp_version`](CheckNSCP.md#check_nscp_version) from the CheckNSCP
module, which exposes `major`, `minor`, `release` and `build` as filterable
keywords, or [`check_nscp_update`](CheckNSCP.md#check_nscp_update) to compare
against the latest release published on GitHub.

The legacy alias `CheckVersion` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_version_samples)
* [Command-line Arguments](#check_version_options)


<a id="check_version_samples"></a>
#### Sample Commands

**Report the running agent version:**

```
check_version
OK: 0.18.1 2026-08-14
```

**As a heartbeat check over NRPE:**

The command never fails on its own, so anything other than OK means the agent
is not answering — which makes it the cheapest possible "is this agent alive"
service.

```
check_nrpe --host 192.168.56.103 --command check_version
OK: 0.18.1 2026-08-14
```

**When you want to alert on the version rather than just report it:**

`check_version` returns a string with no thresholds. Use CheckNSCP's
[`check_nscp_version`](CheckNSCP.md#check_nscp_version) instead, which exposes
the parts as numbers:

```
check_nscp_version "crit=major < 12"
OK: 0.18.1 (2026-08-14)
```



<a id="check_version_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_warning

Just return WARNING (anything passed along will be used as a message).

#### About `check_warning`

`check_warning` is a constant: it always returns **WARNING**, without running
anything. It exists so that a monitoring configuration can be exercised
end-to-end — that the transport works, that the command is allowed, that the
server renders the state — without depending on the health of the host.

The only option is `message=`, which sets the text returned alongside the
status. It defaults to `No message`.

The legacy alias `CheckWARNING` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_warning_samples)
* [Command-line Arguments](#check_warning_options)


<a id="check_warning_samples"></a>
#### Sample Commands

**Return WARNING with the default message:**

```
check_warning
WARNING: No message
```

**Return WARNING with your own message:**

```
check_warning "message=Disk approaching capacity"
WARNING: Disk approaching capacity
```

**Verifying that a monitoring server renders each state correctly:**

Running `check_ok`, `check_warning` and `check_critical` in turn is the quickest
way to confirm that a newly configured service actually shows all three states,
and that notifications fire for the ones you expect.

```
check_nrpe --host 192.168.56.103 --command check_warning --arguments "message=state test"
WARNING: state test
```



<a id="check_warning_options"></a>
#### Command-line Arguments

| Option                            | Default Value | Description       |
|-----------------------------------|---------------|-------------------|
| [message](#check_warning_message) | No message    | Message to return |



<h5 id="check_warning_message">message:</h5>

Message to return

*Default Value:* `No message`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### filter_perf

Run a check and filter performance data.

`filter_perf` while badly named can be used to post process performance data.

It can be useful for sorting performance data or limiting the number of performance data items shown.

In its most basic form you can run `filter_perf command=COMMAND arguments REGULAR ARGUMENTS` for example `check_process`:
```
filter_perf command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: ' ws_size'=0GB;3;5 ' ws_size'=0GB;3;5 ' ws_size'=0GB;3;5 ' ...
```

This will not do anything by itself but we can for instance sort performance data entries by adding `sort=normal`:
```
filter_perf sort=normal command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: 'clion64.exe ws_size'=3.30851GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 ...
```

And further can also limit the number of results shown by adding `limit=5` like so:
```
filter_perf sort=normal limit=5 command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: 'clion64.exe ws_size'=3.30852GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 'msedge.exe ws_size'=0.5757GB;3;5
```

**Jump to section:**

* [Sample Commands](#filter_perf_samples)
* [Command-line Arguments](#filter_perf_options)


<a id="filter_perf_samples"></a>
#### Sample Commands

**Run a check unchanged (no sorting, no limit):**

```
filter_perf command=check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.24464GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used'=29.77734MB;36.98125;41.6039;0;46.22656 '/opt/env-runner used %'=64%;80;90;0;100
```

**Keep only the largest values (`sort=normal`, biggest first):**

```
filter_perf command=check_drivesize sort=normal limit=3
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used %'=64%;80;90;0;100
```

Note that the comparison is on the raw numeric value, ignoring units — `88` (a
percentage) sorts above `8.24` (gigabytes). Mixing units in one sorted, trimmed
set rarely gives you the ranking you meant; filter down to one unit first.

**Sorting the other way is `sort=reverse` — not `reversed`:**

Only `none`, `normal` and `reverse` are recognised, despite the option's own
help text naming `reversed`. Any other value silently leaves the performance
data unsorted, so `limit=` then trims the *original* order rather than the
smallest values — which is exactly what this captured run shows, the first two
counters in the order the check emitted them:

```
filter_perf command=check_drivesize sort=reversed limit=2
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.24916GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100
```

There is no error to tell you the value was ignored, so check the order of what
comes back rather than trusting the flag.

**Top N processes by memory:**

The typical use — a check that emits one counter per process would otherwise
produce hundreds of series.

```
filter_perf sort=normal limit=10 command=check_process arguments "filter=working_set > 0" "warn=working_set > 3G" "crit=working_set > 5G" "detail-syntax=%(exe) ws=%(working_set)"
WARNING: WARNING: clion64.exe=started
'clion64.exe ws_size'=3.30851GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 'msedge.exe ws_size'=0.5757GB;3;5
```

**The status is not affected by `limit`:**

Warning and critical are still evaluated against *every* matching item, so the
alert fires even when the offending series is not among the ones shown.

```
filter_perf command=check_drivesize limit=1 "arguments=crit=used > 50%"
CRITICAL: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
'/ used'=8.30583GB;201.57782;125.98613;0;251.97227
```

Sorting only ever considers numeric counters; entries without a numeric value
keep their original position.



<a id="filter_perf_options"></a>
#### Command-line Arguments

<a id="filter_perf_command"></a>
<a id="filter_perf_arguments"></a>

| Option                      | Default Value | Description                                                 |
|-----------------------------|---------------|-------------------------------------------------------------|
| [sort](#filter_perf_sort)   | none          | The sort order to use: none, normal or reversed             |
| [limit](#filter_perf_limit) | 0             | The maximum number of items to return (0 returns all items) |
| command                     |               | Wrapped command to execute                                  |
| arguments                   |               | List of arguments (for wrapped command)                     |



<h5 id="filter_perf_sort">sort:</h5>

The sort order to use: none, normal or reversed

*Default Value:* `none`

<h5 id="filter_perf_limit">limit:</h5>

The maximum number of items to return (0 returns all items)

*Default Value:* `0`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### render_perf

Run a check and render the performance data as output message.

#### About `render_perf`

`render_perf` runs another check and turns its **performance data into the
message**. The wrapped check's own numbers become filterable records, so you can
list, filter and threshold them from the outside — useful when the check itself
does not expose the value you want to alert on in its message, and when you want
to see what a check is actually emitting.

The check to run is named with `command=`, and its arguments follow (as
`arguments=`, or simply positionally). Because it is a filter check, the full
`filter=` / `warning=` / `critical=` / `top-syntax=` / `detail-syntax=`
vocabulary applies over one record per performance counter.

The default `detail-syntax` renders one tab-separated row per counter — key,
value, unit, warning, critical, min, max — which makes the output easy to read
in a terminal and easy to paste into a spreadsheet. Set `remove-perf=true` when
the rendered message is the point and you do not also want the numbers
duplicated as perf data on the result.

`empty-state` defaults to `unknown`, so a wrapped check that emits no
performance data at all reports UNKNOWN rather than a misleading OK.

See also [`filter_perf`](#filter_perf), which sorts and trims performance data
while leaving the message alone, and [`xform_perf`](#xform_perf), which rewrites
the perf data itself.

**Jump to section:**

* [Sample Commands](#render_perf_samples)
* [Command-line Arguments](#render_perf_options)
* [Filter keywords](#render_perf_filter_keys)


<a id="render_perf_samples"></a>
#### Sample Commands

**Render a check's performance data as the message:**

The result carries two lines: the wrapped check's own result, then the rendered
one. The default `detail-syntax` produces one tab-separated row per counter —
key, value, unit, warning, critical, min, max.

```
render_perf command=check_drivesize
OK: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.24544GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
OK: OK:  / used	8.24544	GB	201.578	226.775	0	251.972	...
```

**Drop the original performance data (`remove-perf=true`):**

Useful when the rendered message is the point and you do not want every number
duplicated as a graphed series.

```
render_perf command=check_drivesize remove-perf=true
OK: WARNING /opt/claude-code: 202.746MB/229.949MB used
OK: OK:  / used	8.24618	GB	201.578	226.775	0	251.972	...
```

**Pick out just the counters you care about:**

`like` is substring matching, so this keeps only the percentage counters.

```
render_perf command=check_drivesize "filter=key like '%'" "detail-syntax=${key}=${value}${unit}"
OK: WARNING /opt/claude-code: 202.746MB/229.949MB used
OK: OK:  / used %=3%, /opt/claude-code used %=88%, /opt/env-runner used %=64%
```

**Threshold on a value the wrapped check does not expose in its message:**

Note that the rendered line lists *every* matching counter, not only the ones
that breached — the default `top-syntax` is `%(status): %(message) %(list)`.

```
render_perf command=check_drivesize "filter=key like 'used %'" "crit=value > 80" "detail-syntax=${key}=${value}%"
CRITICAL: WARNING /opt/claude-code: 202.746MB/229.949MB used
CRITICAL: CRITICAL:  / used %=3%, /opt/claude-code used %=88%, /opt/env-runner used %=64%
```

The wrapped check itself was only WARNING here; the CRITICAL comes from
`render_perf`'s own threshold on the counter value.

**A check with no performance data reports UNKNOWN:**

`empty-state` defaults to `unknown`, so an empty result is distinguishable from
a healthy one rather than reading as OK.

```
render_perf command=check_ok
UNKNOWN: No message
UNKNOWN: UNKNOWN:
```



<a id="render_perf_options"></a>
#### Command-line Arguments

<a id="render_perf_command"></a>
<a id="render_perf_arguments"></a>

| Option                                  | Default Value | Description                             |
|-----------------------------------------|---------------|-----------------------------------------|
| command                                 |               | Wrapped command to execute              |
| arguments                               |               | List of arguments (for wrapped command) |
| [remove-perf](#render_perf_remove-perf) | false         | List of arguments (for wrapped command) |



<h5 id="render_perf_remove-perf">remove-perf:</h5>

List of arguments (for wrapped command)

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                      | Default Value                                          |
|-------------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| <a id="render_perf_filter"></a>[filter](../common-options.md#filter)                                        |                                                        |
| <a id="render_perf_warning"></a>[warning](../common-options.md#warning)                                     |                                                        |
| <a id="render_perf_warn"></a>[warn](../common-options.md#warn)                                              |                                                        |
| <a id="render_perf_critical"></a>[critical](../common-options.md#critical)                                  |                                                        |
| <a id="render_perf_crit"></a>[crit](../common-options.md#crit)                                              |                                                        |
| <a id="render_perf_ok"></a>[ok](../common-options.md#ok)                                                    |                                                        |
| <a id="render_perf_debug"></a>[debug](../common-options.md#debug)                                           | false                                                  |
| <a id="render_perf_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                  |
| <a id="render_perf_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                |
| <a id="render_perf_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                        |
| <a id="render_perf_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                  |
| <a id="render_perf_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                      |
| <a id="render_perf_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | %(status): %(message) %(list)                          |
| <a id="render_perf_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                        |
| <a id="render_perf_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                        |
| <a id="render_perf_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | %(key)	%(value)	%(unit)	%(warn)	%(crit)	%(min)	%(max)
 |
| <a id="render_perf_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | %(key)                                                 |
| <a id="render_perf_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                        |
| <a id="render_perf_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                        |
| <a id="render_perf_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                     |
| <a id="render_perf_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="render_perf_filter_keys"></a>
#### Filter keywords

| Option  | Description                                                          |
|---------|----------------------------------------------------------------------|
| crit    | The critical threshold (range when set, otherwise the numeric bound) |
| key     | The name (alias) of the performance data entry                       |
| max     | The maximum bound of the performance data entry                      |
| message | The name (alias) of the performance data entry                       |
| min     | The minimum bound of the performance data entry                      |
| unit    | The unit of the performance data entry                               |
| value   | The value of the performance data entry                              |
| warn    | The warning threshold (range when set, otherwise the numeric bound)  |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### xform_perf

Run a check and transform the performance data in various (currently one) way.

#### About `xform_perf`

`xform_perf` runs another check and **transforms its performance data** before
returning it. It exists for graphing backends that need something the original
check does not emit. The wrapped check's status and message pass through
untouched.

##### `mode=minmax`

Sets `min=0` and `max=100` on every **percentage** counter — those whose unit is
`%`. Counters with any other unit are left alone. Some graphing systems will not
draw a percentage gauge on a fixed 0–100 axis, or will autoscale it to the
observed range, unless the bounds are declared.

##### `mode=extract`

Copies one field of every counter into a **new, additional** counter, renamed
with `replace=`. The original counters are kept, so the result carries both
series and the graph can show a value against its own bound.

`replace=` is written as `<match>=<replacement>` and is a plain substring
substitution on the counter label, applied everywhere it occurs — so
`replace=used=size` turns `/ used` into `/ size` and `/ used %` into `/ size %`.
It must contain exactly one `=`, or the check returns a syntax error.

**Only `field=max` and `field=min` do anything.** Despite what the option help
suggests, `value`, `warn` and `crit` add no counters at all — the check simply
returns the original performance data unchanged, with no error. If an `extract`
run appears to be a no-op, this is why.

So the useful shape is emitting a counter's declared maximum as its own series:

```
xform_perf command=check_drivesize mode=extract field=max "replace=used=size"
```

An unrecognised `mode=` is an error (`Invalid mode specified`, UNKNOWN), not a
silent pass-through.

See also [`render_perf`](#render_perf), which turns performance data into the
message, and [`filter_perf`](#filter_perf), which sorts and trims it.

**Jump to section:**

* [Sample Commands](#xform_perf_samples)
* [Command-line Arguments](#xform_perf_options)


<a id="xform_perf_samples"></a>
#### Sample Commands

**Emit each counter's declared maximum as its own series (`mode=extract`):**

The original counters are kept and the extracted ones are added alongside,
renamed by substituting `used` with `size` everywhere it appears in the label.

```
xform_perf command=check_drivesize mode=extract field=max "replace=used=size"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.25672GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used'=29.77734MB;36.98125;41.6039;0;46.22656 '/opt/env-runner used %'=64%;80;90;0;100 '/ size'=251.97227GB;201.57782;226.77505;0;251.97227 '/ size %'=100%;80;90;0;100 '/opt/claude-code size'=229.94921MB;183.95937;206.95429;0;229.94921 '/opt/claude-code size %'=100%;80;90;0;100 '/opt/env-runner size'=46.22656MB;36.98125;41.6039;0;46.22656 '/opt/env-runner size %'=100%;80;90;0;100
```

The graph now has a `size` line to draw `used` against.

**`field=min` works the same way; every other field is a silent no-op:**

Despite what the option help suggests, `value`, `warn` and `crit` add nothing —
the performance data comes back exactly as the wrapped check produced it, with
no error to tell you so.

```
xform_perf command=check_drivesize mode=extract field=crit "replace=used=used_crit"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.25701GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used'=29.77734MB;36.98125;41.6039;0;46.22656 '/opt/env-runner used %'=64%;80;90;0;100
```

**Pin percentage counters to a 0-100 axis (`mode=minmax`):**

Only counters whose unit is `%` are touched; everything else is left alone.
`check_drivesize` already declares 0/100 on its percentage counters, so here the
transformation is a no-op — it matters for checks that do not.

```
xform_perf command=check_drivesize mode=minmax
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.2454GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
```

**An unrecognised mode is an error:**

```
xform_perf command=check_drivesize mode=bogus
UNKNOWN: WARNING /opt/claude-code: 202.746MB/229.949MB used
UNKNOWN: Invalid mode specified	...
```



<a id="xform_perf_options"></a>
#### Command-line Arguments

<a id="xform_perf_command"></a>
<a id="xform_perf_arguments"></a>
<a id="xform_perf_mode"></a>
<a id="xform_perf_field"></a>
<a id="xform_perf_replace"></a>

| Option    | Default Value | Description                                                                 |
|-----------|---------------|-----------------------------------------------------------------------------|
| command   |               | Wrapped command to execute                                                  |
| arguments |               | List of arguments (for wrapped command)                                     |
| mode      |               | Transformation mode: extract to fetch data or minmax to add missing min/max |
| field     |               | Field to work with (value, warn, crit, max, min)                            |
| replace   |               | Replace expression for the alias                                            |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                    | Description     |
|---------------------------------------------------|-----------------|
| [/settings/check helpers/alias](#command-aliases) | Command aliases |


### Command aliases <a id="/settings/check helpers/alias"></a>

A list of aliases for already-defined commands (with arguments).
An alias is an internal command that has been predefined to provide a single command without arguments. Be careful so you don't create loops (e.g. check_loop=check_a, check_a=check_loop).
Aliases are also available in CheckExternalScripts under [/settings/external scripts/alias]; use this section when you want aliases without enabling external-script execution. If the same alias name is registered by both modules, the last one to load wins - avoid duplicating definitions.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.





