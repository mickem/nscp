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

    Before 0.17.2 the result never left the agent: the submission was built in a
    way the channels could not read, so the check ran, the command answered
    `Message submitted` and nothing at all reached the monitoring server. If you
    worked around this, the workaround is no longer needed.
