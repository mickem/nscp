# IcingaAgent spike: NSClient++ as a native Icinga 2 agent

Runs an Icinga 2 master and NSClient++ in containers. NSClient++ connects to
the master over the cluster protocol, the way the PowerShell-only
[Icinga for Windows](https://github.com/Icinga/IcingaForWindows) (v27, under
development) does, and runs the checks the master schedules on it. There is no
icinga2 binary and no Icinga configuration on the agent.

```sh
# a Linux build tree with nscp, IcingaAgent, CheckSystem, CheckHelpers,
# CheckDisk and CommandClient built
export NSCP_BUILD_DIR=/path/to/build
./run.sh
docker compose logs -f nscp-agent      # every message, at debug level
docker compose down -v                 # start over (drops both CAs/certs)
```

`ICINGA_IMAGE` and `UBUNTU_IMAGE` override the images (for example
`mirror.gcr.io/icinga/icinga2:latest` when Docker Hub rate-limits you).

## What happens

1. The master starts with `ICINGA_MASTER=1` and the objects in
   `master/nscp-agent.conf`: an Endpoint and Zone `nscp-agent` (no `host`, so
   the agent connects in), a host, and services with
   `command_endpoint = "nscp-agent"`.
2. `run.sh` issues a ticket with `icinga2 pki ticket --cn nscp-agent`.
3. The agent generates a key and a self-signed certificate, connects, sends
   `icinga::Hello` and `pki::RequestCertificate` with the ticket. The master
   answers with `pki::UpdateCertificate` (signed certificate + CA). The agent
   stores both and reconnects, now verifying the master against that CA.
4. The master sends `event::ExecuteCommand` for each service; the agent runs
   the query and answers with `event::CheckResult`. Heartbeats both ways.

## Check commands

Icinga 2.14 added the `ifw-api` CheckCommand for Icinga for Windows. A
CheckCommand that imports it, executed on an endpoint that announces the
`IfwApiCheckCommand` capability (we do), makes the master send:

* `macros.ifw_api_command` - the query to run (defaults to the CheckCommand's
  name, so `object CheckCommand "check_cpu" { import "ifw-api" }` runs
  `check_cpu`);
* `macros.ifw_api_arguments` - the argument definitions, values still holding
  `$macro$` references (`value`, `set_if`, `order`, `skip_key`, `repeat_key`);
* the resolved value of every macro those arguments reference.

The agent expands them like Icinga does and passes them as `key=value` tokens
(leading dashes stripped), so the master-side definition is:

```icinga2
object CheckCommand "nscp-disk" {
  import "ifw-api"
  vars.ifw_api_command = "check_drivesize"
  vars.ifw_api_arguments = {
    "drive"    = { value = "$nscp_disk_drive$" }
    "warning"  = { value = "$nscp_disk_warning$" }
    "show-all" = { set_if = "$nscp_disk_show_all$" }
  }
}
```

A CheckCommand without `ifw-api` (plain `plugin-check-command`) arrives with
only its name and no macros; the agent runs a query of that name without
arguments.

## Which checks the master may run

The core permission policy decides, as for every other transport. The agent
runs every check as `IcingaAgent:<parent endpoint>`, a name it has verified
against the Icinga CA, so `agent/nsclient.ini` grants the master
`CheckSystem.*` and `CheckDisk.*` and the `not allowed` service (a CheckHelpers
command) comes back `Permission denied`. The module's only own switch is
`allow arguments` (default true). It runs no checks on the first connection,
before it holds the CA, and refuses to start without `parent endpoint`.

## Not in the spike

* Listening for the master (we only connect out), several parents/HA.
* Certificate renewal before expiry (the master re-signs on request, so
  sending `pki::RequestCertificate` on every connect should cover it - untested).
* `deadline` / `check_timeout`, `event_command`s, `/v1/actions/execute-command`.
* Reference docs, samples, Windows MSI entry. (The protocol is covered by
  `tests/icinga-agent.test.ts`, against a fake master.)
