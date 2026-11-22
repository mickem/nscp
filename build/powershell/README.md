# Azure test-machine scripts

Hand-run PowerShell helpers for spinning up a throwaway Azure VM, installing a
released NSClient++ on it, running the acceptance suite against it, enrolling it
with a fleet server, and tearing it down again. These are **manual** tooling for
personal/release verification — not a CI pipeline (that can be layered on top
later).

> Requires the Az PowerShell modules. Install them once with
> `./install-azure.ps1`, then connect with `./connect-to-azure.ps1`
> (or the setup scripts will call `Connect-AzAccount` themselves).

> **"was disallowed by Azure … without authenticating through MFA".** Every
> resource creation fails while reads keep working: you are signed in, but the
> token did not do MFA and the tenant enforces it for resource management.
> `connect-to-azure.ps1` warns about this up front; to fix it, sign in again
> answering the MFA challenge, then re-run:
>
> ```powershell
> Connect-AzAccount -Tenant (Get-AzContext).Tenant.Id `
>     -ClaimsChallenge "eyJhY2Nlc3NfdG9rZW4iOnsiYWNycyI6eyJlc3NlbnRpYWwiOnRydWUsInZhbHVlcyI6WyJwMSJdfX19"
> ```
>
> Az caches the context on disk, so a *stale* login is the other half of this
> trap — `Get-AzContext` keeps returning an account whose token expired long ago.
> `connect-to-azure.ps1` verifies the cached context and re-authenticates instead
> of failing on the first `New-Az…` call.

## The flow

```
                install-azure.ps1  (once)   →  Az modules
                connect-to-azure.ps1         →  Azure session
                          │
   ┌──────────────────────┼───────────────────────────────┐
   │ setup-machine.ps1     setup-ubuntu-machine.ps1        │  provision VM,
   │ (Windows)             setup-rocky-machine.ps1 (Linux) │  install NSCP,
   └──────────────────────┼───────────────────────────────┘  nscp web install,
                          │                                   write .vm.pwd
                 npm run test:live    →  live acceptance suite (tests/live)
                          │
                 connect-machine.ps1  →  RDP (Windows) / SSH (Linux) into the VM
                          │
                 show-log.ps1         →  pull nsclient.log / install log
                          │
              teardown-machine.ps1    →  delete the resource group
```

`provision-fleet-machines.ps1` wraps the setup scripts for the fleet case: it
invents a machine name, mints a bootstrap token per machine, and provisions
several of them (see [Build a fleet](#build-a-fleet-machines-enrolled-with-a-fleet-server)).

## Build a fleet: machines enrolled with a fleet server

`provision-fleet-machines.ps1` creates one or more machines that join an
[NSClient fleet server](https://github.com/mickem/nsclient-fleet-server) as they
are installed, so you get an estate to look at in the fleet UI rather than a
single test box.

### Set these two once

The fleet server url and its API key change rarely and belong together, so every
script that talks to a fleet server reads them from the environment and only
needs a flag when you want to override one:

| Variable | Used by | What it is |
| -------- | ------- | ---------- |
| `NSCLIENT_FLEET_SERVER` | everything below (`-FleetServer`) | Fleet server url, e.g. `https://fleet.example.com` |
| `NSCLIENT_FLEET_API_KEY` | provisioning, teardown, `add-nagios-bundle` (`-ApiKey`) | An `nsk_…` key from *API keys* in the fleet sidebar |
| `NSCLIENT_FLEET_SYNC_API_KEY` | `setup-nagios-machine` (`-SyncApiKey`) | Optional: a **view_only** key for the Nagios VM's poller, so an admin key never lands on that box |

Or keep them in a file instead of the environment: copy
`fleet-config.example.json` to `.fleet-config.json` next to it (gitignored)
and fill it in. Each script loads it on start (`Import-FleetConfig` in
`fleet-api.ps1`) and fills in whichever of the three variables are not
already set, so a variable set in the shell still wins over the file, and an
explicit `-FleetServer`/`-ApiKey` wins over both. `NSCLIENT_FLEET_CONFIG`
points at a file somewhere else.

```json
{ "server": "https://fleet.example.com", "apiKey": "nsk_...", "syncApiKey": "" }
```

```powershell
$env:NSCLIENT_FLEET_SERVER  = "https://fleet.example.com"
$env:NSCLIENT_FLEET_API_KEY = "nsk_..."      # API keys, in the fleet sidebar

./provision-fleet-machines.ps1

# A bigger, mixed estate, provisioned concurrently:
./provision-fleet-machines.ps1 -Windows 3 -Ubuntu 2 -Rocky 1 -Parallel

# See what it would create, without touching Azure or the fleet server:
./provision-fleet-machines.ps1 -DryRun -Windows 2 -Ubuntu 2

# Tear it all down again:
./provision-fleet-machines.ps1 -Destroy -RemoveFleetHosts
```

> The individual `setup-*machine.ps1` scripts pick `NSCLIENT_FLEET_SERVER` up
> too, but enrollment there still needs a `-FleetToken`: with the variable set
> and no token they just provision a plain unenrolled VM and say so, rather than
> failing. Passing `-FleetServer` explicitly without a token is still an error —
> that one is a mistake, not a leftover environment.

Each machine gets

- **a plausible name** — `<role>-<site>-<nn>`, e.g. `web-ams-04`, `sql-fra-12`,
  which becomes the VM name, the computer name the fleet server sees and the
  name of its own resource group (`NSCP-Fleet-web-ams-04`). Names are unique
  within a run and against the machines already in the manifest, and stay inside
  the 15-character Windows computer-name limit;
- **its own bootstrap token**, minted from `POST /api/hosts` right before that
  machine is provisioned. Tokens are single-use and expire in about an hour, so
  they are not minted up front for the whole batch;
- **its own resource group and `.vm.<name>.pwd`**, so machines never collide.

Enrollment differs per platform, and both paths end at the same
`agent-state.json`:

| OS      | How it enrolls                                                                                                          |
| ------- | ----------------------------------------------------------------------------------------------------------------------- |
| Windows | `msiexec … FLEET_SERVER=… FLEET_TOKEN=…` — the installer enrolls (0.16+). An older MSI ignores those, and the script then falls back to `nscp enroll` |
| Linux   | `nscp enroll --server … --token …` after the package is installed (the DEB/RPM have no install-time enrollment)          |

Either way the setup script verifies that `agent-state.json` exists before it
reports success — a machine that silently never joined the fleet is exactly the
failure this tooling exists to catch.

> **The setup scripts verify enrollment, they do not repair it.** `nscp enroll`
> runs under `sudo` while the DEB/RPM run the service as `nsclient`, so the
> material it writes has to be handed to that account or the agent enrolls,
> starts its sync, fails to read its own identity and never appears in the
> fleet. The agent does that itself now.
>
> These scripts used to `chown` it here, which fixed the machine and hid the bug
> at the same time — the check that follows would have passed no matter what the
> package did. They now only assert, as the service user, that the manifest is
> readable and the fleet folder writable, and fail the run when it is not.
>
> **So provisioning against a package that predates the fix will now fail**, by
> design, telling you so. Point `-*PackageUrl` at a build that includes it.
> Windows is unaffected (the service runs as LocalSystem).
>
> This failure used to be invisible on the box, because the packaged log file
> was root-owned too. Both are fixed, but `journalctl -u nsclient | grep -i
> fleet` is still the fastest way to see what the sync is doing.

Every run appends to `.fleet-machines.json` (name, OS, resource group, public IP,
fleet host id). Tear the whole estate down again with:

```powershell
./provision-fleet-machines.ps1 -Destroy -RemoveFleetHosts
```

`-RemoveFleetHosts` also deletes the hosts from the fleet server, so a torn-down
VM does not linger there as permanently offline. That needs an admin/owner API
key (an `add_hosts` key may create hosts but not delete them); it is skipped with
a warning if the key is not allowed, and the Azure teardown still runs.

The resource groups are deleted **concurrently**. Deleting one takes minutes and
is almost all waiting on Azure, so a teardown now costs about as long as its
slowest machine instead of the sum of them all — six machines went from ~30
minutes to ~5. (It uses `Remove-AzResourceGroup -AsJob`, so unlike the
provisioning `-Parallel` switch it does **not** need PowerShell 7.) Fleet hosts
are removed first, sequentially, so machines leave the fleet UI immediately
rather than minutes later.

Add `-NoWait` to start every deletion and get your prompt straight back; Azure
finishes them on its own. The manifest is deliberately **kept** in that case,
since nothing was confirmed gone — re-run `-Destroy` later to verify and clear
it (groups that have since disappeared drop out silently, so the second run is
quick).

```powershell
./provision-fleet-machines.ps1 -Destroy -Force -NoWait     # fire and forget
./provision-fleet-machines.ps1 -Destroy -Force             # later: confirm + clear the manifest
```

Useful options: `-Version` (release to install; by default the latest GitHub
release, looked up when the run starts - Linux machines need 0.16.2 or newer,
and the script refuses older releases up front, see above),
`-WindowsPackageUrl` / `-UbuntuPackageUrl` / `-RockyPackageUrl` to install a
build from your own url instead — needed to exercise the installer's own fleet
enrollment before it ships in a release — `-WindowsVersions` (round-robined over
the Windows machines), `-Location`, `-VmSize` (see the quota note below),
`-MaxParallel`, and `-ResourceGroupPrefix`.

> **Your vCPU quota is what caps the estate.** A fresh subscription gets 10
> vCPUs per region, so the estate size is `10 / vCPUs per machine` — and
> machines from an estate you have not torn down still count against it. Every
> VM therefore defaults to a **1-vCPU** size (`Standard_F1as_v7`: 1 vCPU, 4 GB,
> Gen2), which fits **ten** machines in the default quota where a 2-vCPU size
> fits five. Azure only refuses at the VM-create step, after the resource group,
> vnet and public IP exist, so an over-quota machine used to fail ten minutes in
> with a bare `setup exited 1`; the script now checks the regional *and*
> per-family quota up front and stops before creating anything.
>
> Override with `-VmSize` (all the setup scripts take it too). It must be a
> **Generation 2** size — every image these scripts use is Gen2. Watch out for
> `NotAvailableForSubscription`: most classic small sizes (the whole B-series,
> `A1_v2`, `F1s`, `DS1_v2`) are blocked or zone-restricted on a plain
> pay-as-you-go subscription, which is why the default is an F-series v7. The
> preflight rejects a size that is genuinely unavailable, and ignores a merely
> *zonal* restriction (these VMs are created without a zone). Need more than the
> quota allows: tear down an old estate, spread across `-Location`s (the quota is
> per region), or raise it in the portal (*Subscription → Usage + quotas* — on
> pay-as-you-go a modest increase is usually auto-approved).
>
> `-MaxMachines` (default 5) is the cheaper guard in front of that: the run is
> refused before anything is checked against Azure when it, plus the machines
> still in the manifest, would exceed it. Raise it when the quota allows.

> **The VMs enroll from Azure**, so the fleet server url has to be reachable from
> the internet. A `localhost` dev server passes the preflight check on your
> machine and then fails on the VM.

> **A test fleet server without a public certificate:** `-FleetInsecure` allows a
> plain `http://` url, `-FleetNoVerify` additionally skips verification of the
> server certificate, and `-FleetCaFile <pem>` (better) copies your CA to each
> machine and verifies against it. The first two mean the bootstrap token and the
> trust anchors the agent ends up pinning travel over an unauthenticated channel
> — fine for a throwaway test estate, not for anything else.

`fleet-api.ps1` holds the pieces on their own (`New-FleetHost`,
`Remove-FleetHost`, `Test-FleetServer`, `New-FleetMachineName`) if you want to
enroll something by hand:

```powershell
. ./fleet-api.ps1
$h = New-FleetHost -FleetServer https://fleet.example.com -ApiKey $env:NSCLIENT_FLEET_API_KEY
$h.InstallCommand      # nscp enroll --server … --token …
```

The individual setup scripts also take `-FleetServer`/`-FleetToken` directly, if
you want one enrolled machine with a name you choose:

```powershell
./win/setup-machine.ps1 -VmName NSCP-Test -Version 0.15.0 `
    -FleetServer https://fleet.example.com -FleetToken <bootstrap-token>
```

## Role-based estate: one command per suite

`run-estate.ps1` turns the fleet estate into an acceptance lab. A **role** is a
machine that carries a workload next to NSClient++ (MariaDB, SQL Server, IIS);
a **suite** provisions the machines one role needs, waits for them to enroll
with the fleet server and report in sync, collects their logs and tears them
down again:

```powershell
# (NSCLIENT_FLEET_SERVER and NSCLIENT_FLEET_API_KEY are set - see above)
./run-estate.ps1 base                            # today's estate, plus log collection
./run-estate.ps1 mysql,mssql,hub                 # three suites, concurrently
./run-estate.ps1 all -MaxMachines 8              # everything, as the quota allows
./run-estate.ps1 mysql,mssql -Sequential         # one after another, -StopOnFirstFailure applies
./run-estate.ps1 mssql -KeepVms                  # leave it up, then connect-machine.ps1
./run-estate.ps1 -Collect                        # just pull logs from .fleet-machines.json
./run-estate.ps1 all -DryRun                     # the plan, touching nothing
```

At the end it prints the run folder, one line per machine, how long each suite
spent in each stage, and the total runtime (each suite also writes them to
`suite-<role>/timing.json`). A failed machine names the **first stage that
failed**, so the right log is the first one opened:

| Stage       | Meaning                                                      | Look at              |
| ----------- | ------------------------------------------------------------ | -------------------- |
| `provision` | VM create or NSClient++ install failed                       | the run output       |
| `install`   | the role's install fragment exited non-zero (or timed out)   | `role-install.log`   |
| `enroll`    | the agent never enrolled with the fleet server               | `nsclient.log`, `journal.txt` / `events.txt` |
| `sync`      | it enrolled but never reported `in_sync`                     | `fleet.ini`, `fleet-host.json` |
| `bundle`    | the role bundle could not be composed                        | the run output       |
| `check`     | a role check did not return what it should                   | `checks.json`        |

**The gate.** After enroll and sync, a suite composes the role's bundle
(`estate/add-role-bundles.ps1`) and runs `estate/verify-estate.ps1`, in two
stages:

1. **sync** — it polls the fleet server (up to the suite's wait) until
   `GET /api/hosts/<id>/desired` carries the role's bundle (`role-<name>-<estate id>`)
   and the host reports it applied. This is `in_sync` against the state
   *with* the bundle in it, not whatever the host synced before the bundle was
   assigned.
2. **check** — it runs the jest suite `tests/live/estate.test.ts` once for
   every machine that synced. Jest logs in to each agent over REST and runs
   every role check that applies to the machine's OS. Each check must return
   its `expect` state with its `perf` keys, and is retried for
   `-CheckWaitSeconds` (default 120) to wait out a collector warming up or the
   web server restarting after the bundle reload.

Jest is the only check runner: the gate takes each machine's verdict from the
`checks.json` jest writes into its folder (the verdict, the reason and the raw
REST result of every check's last attempt), and adds the sync state to it. It
needs node and `npm install` in `tests/`. The jest output shows one line per
check, and the gate ends with one line per machine:

```
mysql-fra-02: FAILED at check - 1 of 2 check(s) failing; first: check_mysql ...: returned UNKNOWN, expected OK
```

**Running the checks by hand.** Against an estate kept with `-KeepVms`, the
checks are one command from `tests/`. It finds every estate that is still up
(every manifest under `build/powershell` that lists a machine whose
`.vm.<name>.pwd` still exists), so there is no path to type:

```sh
cd tests && npm run test:estate                 # every estate that is up
cd tests && npm run test:estate -- -t mssql     # one role's machines
```

After editing a role's bundle, re-apply it and run the whole gate again:

```powershell
./estate/add-role-bundles.ps1 -ManifestFile <manifest>
./estate/verify-estate.ps1 -ManifestFile <manifest> -WaitMinutes 2
```

**Logs.** Every run writes `.estate-runs/<timestamp>/<machine>/` — on success
and failure alike, before teardown — with `nsclient.log`, `role-install.log`,
`fleet.ini`, `agent-state.json` (private key redacted), `fleet-host.json`,
`journal.txt` (Linux) or `events.txt` and `msi-install.log` (Windows), and any
extra files the role lists. Each suite's `suite.json`, `manifest.json` and
`result.json` sit in `suite-<role>/` next to them. Run
`./estate/collect-logs.ps1 -ManifestFile <manifest>` against a kept estate to
refresh them; it gathers everything into one archive on the VM, so a machine
costs a minute or two of RunCommand round trips, not one per file.

**Cleanup.** Kept estates, suites that died half way and `-NoWait` teardowns
accumulate. `./run-estate.ps1 -Cleanup` (`estate/cleanup-estate.ps1`) tears
down everything the tooling ever left behind: every machine in every manifest
(`.fleet-machines.json` and each run's `suite-<role>/manifest.json`, through
the ordinary `-Destroy -RemoveFleetHosts`), every `NSCP-Fleet-*` resource
group in Azure that no manifest lists, and the fleet host whose hostname is
that machine's name, and the role groups and bundles of estates that are gone.
A stray group younger than `-MinStrayAgeHours` (default 3, from the
`nscp-created` tag the setup scripts put on it) is listed but kept: a suite
still provisioning has no manifest yet, and its groups look like leftovers
(`-IncludeRecent` removes them too). It then removes the run folders nothing
live is left in and the `.vm.<name>.pwd` files of machines that no longer
exist. It prints the
plan and asks first; `-DryRun` only prints it, `-Force` skips the question,
`-KeepLogs` keeps the run folders. Whatever could not be removed stays in its
manifest, so the next `-Cleanup` picks it up.

```powershell
./run-estate.ps1 -Cleanup -DryRun     # what would go
./run-estate.ps1 -Cleanup -Force      # remove it all
```

**Concurrency and the quota.** Suites run concurrently by default, each as
its own process (it works on Windows PowerShell 5.1 too). A suite starts only
while the machines in flight stay within `-MaxMachines` (default 5), so `all`
queues instead of failing ten minutes in when the quota runs out.
`-MaxParallel` (default 4) caps the suites in flight. `-Sequential` runs them
one after another in the order given; it is the only mode where
`-StopOnFirstFailure` applies, and it also copies each machine's credentials to
the shared `.vm.pwd`, so `connect-machine.ps1` finds the latest one. `-KeepVms`
prints the `-Destroy` command for each suite's manifest.

`-Version`, `-*PackageUrl`, `-Location`, `-WindowsVersions` and the `-Fleet*`
switches are passed on to `provision-fleet-machines.ps1`, which also takes the
roles directly: `-Roles mysql=1,base=2`. There, `-Windows`/`-Ubuntu`/`-Rocky`
are shorthand for that many `base` machines on one family.

### Adding a role

A role is one folder under `estate/roles/`; nothing in the runner changes.

```
estate/roles/mysql/
  role.json            # the descriptor below
  install-ubuntu.sh    # one install fragment per OS it supports
  install-rocky.sh
```

```json
{
  "name": "mysql",
  "description": "MariaDB server on Linux, checked with CheckMySQL",
  "os": ["ubuntu", "rocky"],
  "machines": 1,
  "vcpus": 1,
  "vmSize": null,
  "image": null,
  "modules": ["CheckMySQL"],
  "install": { "ubuntu": "install-ubuntu.sh", "rocky": "install-rocky.sh" },
  "installTimeoutMinutes": 10,
  "tag": { "role": "mysql" },
  "bundle": {
    "settings": { "mysql": { "hostname": "127.0.0.1", "user": "nscp", "password": "${ROLE_PASSWORD}" } }
  },
  "checks": [
    {
      "command": "check_mysql",
      "args": { "warning": "connections_pct > 100", "critical": "uptime < 1s" },
      "expect": "OK",
      "perf": ["mariadb_uptime", "mariadb_connections_pct"]
    }
  ],
  "logs": { "ubuntu": ["/var/log/mysql/error.log"], "rocky": ["/var/log/mariadb/mariadb.log"] }
}
```

| Key | Meaning |
| --- | ------- |
| `name` | Must match the folder: lowercase letters and digits. It becomes the machine-name prefix (`mysql-fra-02`), so a role that runs on Windows gets at most 8 characters (the computer name is capped at 15). `base` keeps the made-up names. |
| `os` | Any of `windows`, `ubuntu`, `rocky`; a suite's machines round-robin over it. |
| `machines` | How many machines a suite provisions (default 1); `run-estate.ps1 mysql=2` overrides it. |
| `vcpus`, `vmSize` | What the quota preflight counts, and a size override for a role that needs more than the 1-vCPU default. |
| `image` | `{ "publisher", "offer", "sku", "version"? }`: a Marketplace image instead of the family default (Windows-only roles), e.g. SQL Server 2022 Developer for `mssql`. |
| `modules` | Activated **at install time**. A bundle can enable a module in `fleet.ini` but cannot load it into a running agent, so this is what lets the bundle take effect on the first sync. |
| `install` | Per OS, a fragment run as root/SYSTEM on the VM **before** NSClient++ is installed, with `ROLE_PASSWORD` in its environment and `installTimeoutMinutes` as its limit. Its output lands in `/var/log/nscp-role-install.log` or `C:\nscp-role-install.log`. Exit non-zero to fail the machine at the `install` stage. |
| `tag` | Operator-set tags put on the fleet host **before** it enrolls, so its first desired state already carries the role's group. Required when the role has a `bundle`: the group selects on it, and an empty selector would match every host in the tenant. |
| `bundle` | `{ "settings": { ... } }`: the nested object path is the INI section path in `fleet.ini` (`settings.mysql` → `[/settings/mysql]`); `${ROLE_PASSWORD}` is substituted, `${ESTATE_IPS:<role>}` becomes the public IPs of that role's machines in the suite (comma-separated: an allow-list for a server only the hub may reach), and any other `${...}` is refused. The role's `modules` are enabled in the same bundle. `"scripts": "<folder>"` names a folder under the role whose files ship as the bundle's `scripts/` entries, laid out as the agent's own scripts folder is (`python/`, `lua/`, and the external scripts at the top); the agent stages them under `${fleet-folder}/scripts`, and the bundle's settings then point the script modules at them with `[/settings/default] script root = fleet`. Such a bundle is built by the runner and uploaded whole (`POST /api/bundles`) instead of composed on the server. |
| `checks` | What the gate runs over REST. `args` are passed as `key=value` (an array repeats the key); `expect` is a Nagios state, or `any` for any state but UNKNOWN; every name in `perf` must occur in some perfdata key (case-insensitive, so `used %` matches `C:\ used %`); `message` is text the check's message must contain (case-insensitive), for a check whose proof is what it prints rather than a perfdata key; `os` limits a check to some families (`linux` means Ubuntu and Rocky); `each: "<role>"` runs it once per machine of that companion role, with `${target.ip}`, `${target.name}` and `${target.password}` (its web password) filled in. |
| `rest` | Requests to the agent's own API, run in order, for what a role delivers that no check reports. Each has a `path` (`/api/...`), a `method` (`GET` by default, `PUT` with a text `body`, or `DELETE`), an optional `status` (default 200), `contains` (text the body must hold; required with a 200), `excludes`, and `os` as on a check. Matching ignores case and reads a backslash as `/`, so one entry covers a Windows and a Linux listing. A GET is retried like a check; a PUT or DELETE only on a dropped connection or a lost session, never after it was answered. Recorded in `checks.json` as `<method> <path>`. |
| `faults` | Misbehaviour to inflict and have the fleet server explain, run one at a time after the checks. Each has a `name`, `start` and `clear` (commands on the agent, usually scripts the bundle ships), a `check` (`command`, `args`, `expect`, default `CRITICAL`) that must fail while the fault is on and return OK after it is cleared, and `explain` with `mentions` (the explanation must name one) and `culprit` (recorded only); `os` as on a check. Needs the fleet server and an API key (see `faults` below). |
| `localSettings` | `{ "<settings path>": { "<key>": "<value>" } }`, written into the host's own `nsclient.ini` with `nscp settings --set` right after the modules are activated. Use it only for a key the installer itself writes: a local key beats the same key from a bundle (`fleet.ini` is an include, and local settings win). |
| `companions`, `companionOnly` | `{ "<role>": <count> }`: machines of other roles a suite of this one provisions alongside it (a hub's targets). `companionOnly: true` keeps a role out of `all`; it only comes with the role that needs it. |
| `logs` | Per OS, extra files the log collection picks up (saved as `role-<file>`). |

**Writing a check that checks something.** The gate runs against
unmanipulated machines, so a check must not depend on host state: pin
`warning`/`critical` to something that cannot trip (`load > 100`,
`used > 100%`) rather than leaving the defaults, which can (`check_uptime`
warns for the first two days of a fresh VM). Pin them rather than setting
`none`: perfdata is only emitted for the variables a threshold references, so
`none` also silently drops every `perf` key. Then ask for `expect: "OK"` and
the perf keys — `any` with no perf only proves the command answers. The
forced WARNING/CRITICAL cases stay in the self-spawning `tests/` suites.

`ROLE_PASSWORD` is generated once per role in an estate — every machine of a
role shares it, since one bundle carries it to all of them, and a role that
already has machines in the manifest keeps theirs — and is recorded only in
each machine's `.vm.<name>.pwd`. It reaches the setup script through the
environment (`NSCP_ROLE_PASSWORD_<ROLE>`), not its command line, and
`suite.log` masks credential lines (`ConvertTo-EstateRedactedLine`). It is a
throwaway secret on a throwaway estate — never reuse the pattern for anything
real.

**Estates.** Every manifest is an estate with its own id (printed when it is
provisioned). Its machines are tagged `estate=<id>` and its role groups and
bundles are named `role-<role>-<id>`, selecting on the role's tags and the
estate tag, so two estates of the same role — a kept `-KeepVms` estate and a
new run, or two suites at once — never share a bundle, a password or an
allow-list. `-Destroy -RemoveFleetHosts` removes the estate's groups and
bundles with its last machine.

### Shipped roles

| Role | Machine | What the gate proves |
| ---- | ------- | -------------------- |
| `base` | Windows + Ubuntu, NSClient++ only | Enrollment, sync and the core checks (today's estate) |
| `mysql` | MariaDB on Ubuntu or Rocky | `check_mysql`, `check_mysql_query` through a `[/settings/mysql]` bundle |
| `mssql` | Marketplace SQL Server 2022 Developer on Windows Server 2022 | All 13 CheckMSSQL commands, OK with perfdata, through a `[/settings/mssql]` bundle |
| `hub` | Ubuntu with CheckNet, NRPEClient and NSCPClient, plus two `target` machines (Windows and Ubuntu) | `check_nrpe`, `check_remote_nscp` and `check_nsclient_web_online` from the hub to each target |
| `scripts` | Windows, Ubuntu and Rocky with CheckExternalScripts, LUAScript and PythonScript | A bundle that ships a script of each kind plus the file each one depends on (a Python and a Lua helper in `lib`, a second shell/batch script called by relative path), run from the fleet tree through `[/settings/default] script root = fleet`; each check's message proves the dependency was reached |
| `faults` | Windows and Ubuntu with a stand-in vendor app, *Acme Agent*, that misbehaves on demand | Four faults - a CPU hog, a memory hog, a full disk and a crash-looping service - each fail their check, reach the fleet server as failure context with a metrics snapshot, and are explained by `POST /api/explain` from that report, the snapshot and the host's facts |

Notes on `mssql`:

- There is no SQL Server 2022 **Express** image on Windows Server 2022
  (`sql2022-ws2022` offers `enterprise`, `standard`, `web` and `sqldev`).
  Developer is free on Azure too, and it has SQL Agent, so `check_mssql_jobs`
  reads real jobs.
- The fragment enables TCP 1433 and mixed-mode authentication, then restarts
  the engine in single-user mode (`-m`, restricted to its own application
  name) to create the `nscp` login. SYSTEM is not a sysadmin on the image, but
  every local administrator is in single-user mode, so this does not depend on
  how the image provisioned its logins.
- It installs ODBC Driver 18, which encrypts by default against the
  instance's self-signed certificate. Every check therefore passes
  `trust-cert=true`. TLS trust is a per-check option and cannot go in
  `[/settings/mssql]`, which also exercises a valued boolean over REST.
- `installTimeoutMinutes` is 20: the fragment restarts the engine twice and
  downloads the driver.

Notes on `scripts`:

- One bundle serves all three operating systems: it carries both `fleet_a.sh`
  and `fleet_a.bat`, registers `fleet_ext_sh` (`sh scripts/fleet_a.sh`) and
  `fleet_ext_bat` (`scripts\fleet_a.bat`), and the role's `os` filter picks
  the check. The shell script is run through `sh` because the sync writes
  files without an execute bit.
- Nothing in the bundle names a host path: `script root = fleet` is
  `${fleet-folder}` wherever that is, and the external script is started
  there, so `scripts/fleet_b.sh` resolves from inside `fleet_a.sh` exactly as
  `scripts\check.bat` resolves from the installation directory today.
- Its `rest` reads list each runtime under `/api/v2/scripts` (`ext`, `lua`,
  `py`) and show each runtime's `lib` helper or second script by a name
  relative to the runtime's folder. Those files exist only in the fleet
  tree, so the listings and the show sandbox are proven to follow the root.
- It then adds a script to each runtime over REST (`PUT /api/v2/scripts/...`),
  lists and shows it, and deletes it again: the add must record it relative
  to the root (`scripts/`, `lua/`, `python/`) and the delete must name a file
  under `fleet/scripts`. External scripts take two deletes - the definition
  by its alias, then the file by its path. The local twin below also drives
  the same steps through the `nscp ext-scr|lua|py` command line.
- The role itself also runs on every CI build, without Azure:
  `tests/estate-roles-local.test.ts` builds its bundle the way the estate does
  (`tests/src/estate-roles.ts`, and where PowerShell is installed it asserts
  `New-EstateBundleZip` produces the same zip), serves it from a fake fleet
  server to a local agent, and judges the role's `checks` and `rest` with the
  functions `tests/live/estate.test.ts` uses. A change that breaks the role's
  descriptor fails there first; an estate run is left to prove the real fleet
  server, the packages and the operating systems.

Notes on `faults`:

- It tests the fleet server as much as the agent: what an operator gets back
  when they paste an alert into a host's **Context** tab. It needs a fleet
  server with failure context and explanations, and an agent build that sends
  failure context (`-FromMain` or `-BuildRunId` until a release has it). On
  the tenant, explanations must be switched on with a provider and a key
  (*Settings* in the fleet console, or `PUT /api/settings/explain`), and its
  `min_interval_secs` must stay at or below half of
  `NSCP_ESTATE_FAULT_WAIT_SECONDS` (150 s by default). A run costs one model
  call per fault per machine, so eight calls in all. When the tenant is not
  ready, every fault fails and says why before anything is switched on.
- The install fragment puts in a fake vendor app, *Acme Agent*. On Ubuntu it
  is a real `.deb` (`acme-agent`). On Windows it is two programs compiled
  from C# with `Add-Type` plus an *Uninstall* registry entry. Either way it
  shows up in `software.installed`, and its `acme-sync` service in
  `services.installed`. The bundle turns those fact sets on, together with
  `os`, `hardware`, `network.interfaces` and `storage.volumes`. The CPU and
  memory hogs run under Acme's own process names (`acme-indexer`,
  `acme-cache`), so a model reading the snapshot's process list can name
  the culprit.
- The bundle ships `fault.sh` / `fault.bat` (with `fault.ps1`), registered as
  `fault_<cpu|memory|disk|service|clear>_<sh|bat>`. jest runs them through
  the agent itself, so a fault is switched on without RunCommand or SSH. A
  started fault is detached from the agent: in its own session on Linux,
  created through WMI on Windows. Each one also ends by itself after an hour.
  The Linux agent runs them as `nsclient`, so `/var/lib/acme` is
  world-writable, and `acme-sync` crash-loops on a trigger file and heals
  itself once the file is removed. No step needs root.
- `localSettings` sets `capture metrics = true` and `prune metrics = zeros,
  empty, duplicates` under `[/settings/core/failed checks]`, and
  `failure context interval = 20s` under `[/settings/fleet]`. The interval is
  read only at start, which is why it is a local key and not part of the
  bundle.
- No scheduler runs on the machine, so the only failed checks are the ones
  jest runs. The faults run one at a time:
  1. `clear` switches off anything a previous run left on.
  2. The fault is switched on.
  3. Its check runs every 10 s until it fails, and keeps running until the
     fleet server holds a report of it in that state with
     `context_state = captured`.
  4. `POST /api/explain` is called with the alert a monitoring system would
     have raised.
  5. The fault is cleared, and its check must come back OK before the next
     fault starts. That way each explanation's snapshot shows only its own
     fault.
- An explanation passes when it rests on the right evidence and names what
  the fault is about. The right evidence is the check's own report, a
  snapshot and the agent's facts. "What the fault is about" is a `mentions`
  word: `cpu`, `memory`, `disk`, `acme-sync`. Whether it also names the
  culprit (`culprit`) is recorded, not required. The point is to see what
  comes back: jest prints every explanation, and the gate keeps them in
  `<machine>/explanations.json` next to `checks.json`.

Notes on `hub`:

- The targets run NRPE over TLS with the agent's own certificate
  (`insecure = false`) and `allow arguments` on. Their bundle sets `allowed
  hosts` to the hub's public IP only (`${ESTATE_IPS:hub}`), so port 5666 is
  open in the NSG but answers nobody else.
- `verify mode = none` (no client certificate) is a `localSettings` key, not
  a bundle one. A fresh MSI install writes its secure NRPE preset,
  `verify mode = peer-cert`, into `nsclient.ini`, where it beats anything a
  bundle sends; the Linux packages leave NRPEServer on its own default. Without
  it the Windows target refuses the hub ("Failed to verify other ends
  certificate"). The preset does not touch `allowed hosts`, so the bundle's
  hub-only list applies on both platforms.
- `check_remote_nscp` and `check_nsclient_web_online` reach the target's REST
  API with its web password and `verify=none`: the agents' certificates are
  self-signed. `checks.json` records `${target.password}`, not the password.
- `target` is `companionOnly`: `all` provisions it only with the hub.

**`-CheckNsclient`.** `./run-estate.ps1 <suite> -CheckNsclient` also runs every
role check through the `check_nsclient` plugin from the runner, the way a
Nagios server does: `check_nsclient nsclient auth login` to one profile per
machine, then `queries execute-nagios <command> key=value...`, judged on its
exit code and perfdata, and the profile is logged out again. The plugin is
`check_nsclient` on PATH, else the one an NSClient++ install puts in its
program folder (`verify-estate.ps1 -CheckNsclientPath` picks another).

## Turn-key Nagios monitoring for the fleet

The [`nagios/`](nagios/) scripts extend the fleet estate with a classic
monitoring server: a Nagios Core 4 VM that the fleet machines report into over
NRDP, with hosts registered in Nagios automatically. Four stages, each one
command:

```powershell
# (NSCLIENT_FLEET_SERVER and NSCLIENT_FLEET_API_KEY are set - see above)

# 1. Nagios server VM (nagios4 + NRDP + the host-registration timer)
$env:NSCLIENT_FLEET_SYNC_API_KEY = "nsk_..."       # a view_only key - it lives on the VM
./nagios/setup-nagios-machine.ps1

# 2. the estate (existing tooling, nothing new)
./provision-fleet-machines.ps1 -Windows 2 -Ubuntu 2

# 3. the bundle: every agent starts submitting passive results over NRDP
./nagios/add-nagios-bundle.ps1

# 4. watch it converge (hosts in Nagios, all services reporting)
./nagios/verify-nagios-estate.ps1 -WaitMinutes 15
```

How the pieces fit:

- **`setup-nagios-machine.ps1`** provisions an Ubuntu VM (ports 22 + 80),
  installs Ubuntu's `nagios4` + Apache and the NRDP receiver (random token,
  random `nagiosadmin` password), and writes everything the later stages need
  to `.nagios.pwd`. With `-FleetServer` it also installs **fleet-nagios-sync**
  ([`nagios/fleet-nagios-sync.sh`](nagios/fleet-nagios-sync.sh)): a one-minute
  systemd timer that polls `GET /api/hosts` and mirrors every *enrolled* host
  into `/etc/nagios4/conf.d/fleet/` as a passive host with one service per
  entry in [`nagios/passive-checks.json`](nagios/passive-checks.json),
  validating with `nagios4 -v` (and rolling back) before every reload. Hosts
  deleted from the fleet leave Nagios on the next sync; a fleet server that is
  briefly unreachable changes nothing.
- **`add-nagios-bundle.ps1`** composes a signed bundle on the fleet server
  (`POST /api/bundles/compose`) that enables `NRDPClient` + `Scheduler`, points
  them at the Nagios VM's NRDP url/token (from `.nagios.pwd`), and adds one
  schedule per catalog entry - the schedule name is the service name Nagios
  expects, which is why both sides read the same `passive-checks.json`. The
  bundle is assigned to a group matching **every** host (created as
  `nagios-monitoring` if missing); re-running composes a new version and moves
  the assignment over.
- **`verify-nagios-estate.ps1`** is the end-to-end gate: it polls until every
  enrolled fleet host exists in Nagios *and* every service has received a
  passive result, and exits non-zero otherwise.
- Services use **freshness checking**: an agent that stops reporting goes
  CRITICAL ("stale") after three intervals - dead machines are noticed, not
  just silent.

> **A bundle cannot switch a module on in a running agent.** Enabling a module
> in a bundle writes it into `fleet.ini`, and the reload that follows re-reads
> settings for the plugins already loaded — it does **not** load a newly enabled
> one. The host reports *in sync*, `fleet.ini` looks perfect, and nothing is ever
> submitted until the service restarts. The fleet setup scripts therefore
> activate `NRDPClient`, `IcingaClient` and `Scheduler` at install time, so a
> machine they provision is ready for the Nagios (or Icinga) bundle whenever it
> arrives. **A host enrolled
> some other way needs `NRDPClient` and `Scheduler` enabled locally, or one
> restart after the bundle lands** (`systemctl restart nsclient` /
> `Restart-Service nscp`). Symptom to recognise: `verify-nagios-estate.ps1`
> reporting hosts present in Nagios but no service ever receiving a result.

Two constraints worth knowing:

- **Host names must line up.** Nagios knows a host by the hostname the agent
  reported at enrollment; the agent submits NRDP results as its OS host name
  (`hostname = auto`). Those are the same string for machines these scripts
  provision, but a host enrolled under a chosen name (`nscp enroll --hostname`,
  the MSI's `FLEET_HOSTNAME`) submits under a different one and stays "stale"
  in Nagios.
- **The catalog is frozen on the Nagios VM.** `setup-nagios-machine.ps1` bakes
  `passive-checks.json` (service names, and the freshness threshold = 3× its
  interval) into the VM, while `add-nagios-bundle.ps1` re-reads it. After
  editing the catalog, re-run the setup script (or edit
  `/etc/nagios4/fleet-services.list` + the template by hand) before
  re-publishing the bundle.

API keys: the sync timer only reads, so give it a dedicated **view_only** key
(`NSCLIENT_FLEET_SYNC_API_KEY`) - it is stored on the Nagios VM. Composing the
bundle and creating the group need an **owner/admin** key
(`NSCLIENT_FLEET_API_KEY`).

Teardown: `./teardown-machine.ps1 -ResourceGroupName NSCP-Nagios-RG` for the
VM; the `nagios-nrdp` bundle and `nagios-monitoring` group are removed in the
fleet UI (unassign, then delete) if you no longer want agents submitting.

> **Plain HTTP.** NRDP and the Nagios UI listen on port 80: the NRDP token and
> the `nagiosadmin` digest login travel unencrypted, and the passive results do
> too. Same stance as `-FleetInsecure` - fine for a throwaway test estate, not
> for anything else.

> **Nagios versions.** Ubuntu's `nagios4` package (4.4.x) plus the latest NRDP
> release is deliberately the boring choice. The Icinga 2 variant of the same
> flow is [below](#turn-key-icinga-2-monitoring-for-the-fleet).

## Turn-key Icinga 2 monitoring for the fleet

The [`icinga/`](icinga/) scripts are the same flow against an Icinga 2 server:
the fleet machines submit passive check results to its REST API
(`/v1/actions/process-check-result`) with the `IcingaClient` module, driven by
`Scheduler`, and all of it is configured through a fleet bundle. Four stages,
each one command:

```powershell
# (NSCLIENT_FLEET_SERVER and NSCLIENT_FLEET_API_KEY are set - see above)

# 1. Icinga 2 server VM (icinga2 + API listener + the templates agents create objects from + Icinga Web 2)
./icinga/setup-icinga-machine.ps1

# 2. the estate (existing tooling, nothing new)
./provision-fleet-machines.ps1 -Windows 2 -Ubuntu 2

# 3. the bundle: every agent starts submitting passive results to Icinga
./icinga/add-icinga-bundle.ps1

# 4. watch it converge (hosts in Icinga, every check reporting)
./icinga/verify-icinga-estate.ps1 -WaitMinutes 10
```

How the pieces fit:

- **`setup-icinga-machine.ps1`** provisions an Ubuntu VM (ports 22 + 5665,
  and 80 for Icinga Web 2),
  installs Ubuntu's `icinga2`, builds the Icinga CA and node certificate and
  enables the API listener. It defines two `ApiUser`s — `nscp` for the agents
  (process check results, query and create `Host`/`Service`, nothing else) and
  `admin` for you and the verify script — and two templates, `fleet-host` and
  `fleet-passive-service`. It then walks the agent's own path as the `nscp`
  user (create a host and a service from the templates, submit a result, read
  it back as a passive one, fail to delete it) and writes everything the later
  stages need to `.icinga.pwd`.
- **Nothing registers hosts on the Icinga VM.** The bundle sets
  `ensure objects = true`, so each agent creates its own `Host` and `Service`
  objects from those templates the first time it submits. That is the part of
  `IcingaClient` this flow is there to exercise, and it is why this stage
  needs no fleet API key and no sync timer.
- **`add-icinga-bundle.ps1`** composes a signed bundle on the fleet server that
  enables `IcingaClient` + `Scheduler`, points `[/settings/icinga/client/targets/default]`
  at the Icinga VM (from `.icinga.pwd`) and adds one schedule per entry in
  [`icinga/passive-checks.json`](icinga/passive-checks.json) on the `ICINGA`
  channel. The schedule name is the Icinga service name; `host_check` is
  submitted as the result of the host itself, so it is what holds the host UP.
  The bundle is assigned to a group matching **every** host (created as
  `icinga-monitoring` if missing); re-running composes a new version and moves
  the assignment over. `-DryRun` prints the configuration, password masked.
- **`verify-icinga-estate.ps1`** is the end-to-end gate: it polls until every
  enrolled fleet host exists in Icinga, the host has a passive `host_check`
  result and every catalog service has a passive result, and exits non-zero
  otherwise. It requires the machines in `.fleet-machines.json` when that
  manifest exists, so other hosts enrolled with the same fleet server (your
  own desktop, say) do not fail it; `-Hosts a,b` names them instead and
  `-AllHosts` requires every enrolled host.
- **Freshness** is done the Icinga 2 way, since it has no
  `freshness_threshold`: the templates use the `dummy` check command with a
  check interval of three submission intervals. A passive result pushes the
  next check out by that long; when nothing arrives Icinga runs the dummy
  itself and the object goes DOWN/CRITICAL ("stale"). Both the self-test and
  the gate therefore only count a result with `last_check_result.active ==
  false` — the dummy leaves a result too.

Things worth knowing:

- **One monitoring server per host.** The Nagios and the Icinga bundle name
  their schedules after the service (`cpu`, `memory`, ...) and a schedule has
  one channel, so a host given both reports each shared schedule to one server
  only. Unassign the `nagios-nrdp` bundle before an Icinga run (and the other
  way round); `add-icinga-bundle.ps1` warns when `nagios-monitoring` still has
  one.
- **The modules must already be loaded.** As with Nagios, a bundle cannot load
  a module into a running agent, so the setup scripts activate `IcingaClient`
  (next to `NRDPClient` and `Scheduler`) at install time. A machine
  provisioned before that was added needs one service restart after the bundle
  lands.
- **Host names must line up**, exactly as in the Nagios flow: the agent creates
  and reports under its OS host name (`hostname = auto`), and the gate looks
  for the hostname the fleet server recorded.
- **Hosts are never removed from Icinga.** An agent can create its objects but
  not delete them, and nothing mirrors the fleet's host list here. A machine
  that is torn down stays behind as a stale host; the VM is a throwaway, or
  remove one with
  `curl -k -u admin:<pw> -H 'Accept: application/json' -X DELETE 'https://<ip>:5665/v1/objects/hosts/<name>?cascade=1'`.
- **The freshness window is frozen on the Icinga VM.** It is three times the
  catalog's `interval` at provisioning time; after changing the interval,
  re-run the setup script or edit `/etc/icinga2/conf.d/fleet-templates.conf`.
  Adding or removing catalog entries needs only a new bundle.

Teardown: `./teardown-machine.ps1 -ResourceGroupName NSCP-Icinga-RG` for the
VM; the `icinga-passive` bundle and `icinga-monitoring` group are removed in
the fleet UI (unassign, then delete).

> **Unverified TLS.** The API serves a certificate from the VM's own Icinga CA,
> issued for the VM's name rather than its address, so the bundle sets
> `verify mode = none`: the agents send the `nscp` API password to whoever
> answers at that address (each agent logs that once). Same stance as
> `-FleetInsecure` - fine for a throwaway test estate, not for anything else.
> Against a real Icinga server pass `-VerifyMode peer-cert` and give the
> machines its CA.

> **Icinga Web 2 is on plain http.** The setup script also installs Icinga
> Web 2 with Icinga DB (MariaDB, Redis, the `icingadb` daemon, Apache - all
> Ubuntu 24.04 packages) and opens port 80: browse to
> `http://<ip>/icingaweb2/` and log in with the `Icinga Web User` /
> `Icinga Web Password` lines of `.icinga.pwd` (a different password from the
> API's `admin`). The login travels unencrypted, like everything else about
> this throwaway box. `-NoWeb` leaves the UI, the database and port 80 out;
> `-AddWeb` installs it on the VM an earlier run left in `.icinga.pwd`,
> without recreating the VM or changing its address. There is no Director
> either way.

### Agent mode: the master schedules the checks

The same Icinga VM can run the estate the other way round, with NSClient++
acting as a native Icinga agent (the experimental `IcingaAgent` module, see
[the scenario](../../docs/docs/scenarios/icinga-agent.md)): the agent connects
to the master on 5665, the master sends each check down that connection
(`command_endpoint`) and gets the result back within seconds.

```powershell
# (NSCLIENT_FLEET_SERVER and NSCLIENT_FLEET_API_KEY are set - see above)
$env:NSCLIENT_FLEET_SYNC_API_KEY = "nsk_..."   # a view_only key, for the Icinga VM

# 1. Icinga 2 server VM - the same one as for the passive flow
./icinga/setup-icinga-machine.ps1

# 2. the estate, from a build that has the IcingaAgent module
./provision-fleet-machines.ps1 -Windows 2 -Ubuntu 2 -WindowsPackageUrl <msi> -UbuntuPackageUrl <deb>

# 3a. the master's side: check command, templates, services and the timer
#     that registers every enrolled fleet host and signs its certificate
./icinga/setup-icinga-agent-sync.ps1

# 3b. the agents' side: a bundle that points IcingaAgent at the master
./icinga/add-icinga-agent-bundle.ps1

# 4. watch it converge (hosts UP, every check answered by the agent)
./icinga/verify-icinga-estate.ps1 -Mode agent -WaitMinutes 10
```

How it differs from the passive flow:

- **The master holds everything.** An agent cannot register itself here: the
  master needs an `Endpoint`, a `Zone` and a `Host` for it before it will talk
  to it. `setup-icinga-agent-sync.ps1` installs `fleet-icinga-agent-sync`, a
  one-minute timer on the VM (the counterpart of the Nagios flow's sync) that
  reads the fleet server's host list and renders those three objects per
  enrolled host into `/etc/icinga2/conf.d/fleet-agents/`, validating before it
  reloads and rolling back when the result does not validate.
- **The checks are in [`icinga/agent-checks.json`](icinga/agent-checks.json)**,
  not in the bundle. Each entry becomes an `apply Service` rule on the master;
  `arguments` are NSClient++ tokens (`warning=load > 80`) sent along with the
  query. After editing the file re-run `setup-icinga-agent-sync.ps1` - no new
  bundle is needed.
- **No tickets.** A ticket is issued per host name and a bundle is the same
  for every host, so the agents ask for a certificate without one and the
  timer signs the pending request of every host it registered (`icinga2 ca
  sign`), usually within a minute.
- **The agents verify the master from the first connection.** The bundle pins
  the SHA-256 fingerprint of the master's certificate (`parent fingerprint`)
  and sets `trust on first use = false`; once an agent holds the CA the pin no
  longer matters.
- **The gate asks for more than a result.** `-Mode agent` requires every host
  UP (`cluster-zone`: its agent is connected) and every service's last result
  to carry the agent as `check_source` - while an agent is away Icinga answers
  in its place, and that must not pass.

Things worth knowing:

- **`IcingaAgent` is not in a release yet.** The machines need a build of
  `main`. The setup scripts activate the module at install time when the
  package ships it; as with the other flows, a bundle cannot load a module
  into a running agent.
- **One mode per host.** The passive flow creates a host of the same name
  through the API, and Icinga cannot hold both. The timer skips a host that
  exists that way and logs it; `setup-icinga-agent-sync.ps1 -ReplacePassive`
  lets it delete the passive one instead. Unassign the `icinga-passive` bundle
  first, or its agents create the host again (`add-icinga-agent-bundle.ps1`
  warns).
- **Hand-written hosts are left alone.** A fleet host whose `Endpoint`, `Zone`
  or `Host` is already defined in another file under `/etc/icinga2` is
  skipped, since a second definition fails validation for everything.
- **Endpoint names are the lower-cased host name**, which is what the agent
  calls itself by default and what its certificate is issued for.
- **Removed hosts go away here.** Unlike the passive flow, a host that leaves
  the fleet server loses its objects on the next timer run.
- **Anyone enrolled is signed.** The timer signs whatever asks in the name of
  an enrolled fleet host. Fine for a throwaway estate; a real master hands out
  tickets or signs by hand.
- **Look at the timer** with `journalctl -u fleet-icinga-agent-sync` on the VM.

Teardown: unassign and delete the `icinga-agent` bundle and the
`icinga-agents` group in the fleet UI; the VM goes with its resource group.

### Explained alerts: the tutorial, as a test

The tutorial *Icinga alert emails that explain themselves* has an agent submit
passive results to Icinga 2 and report every failed check to the fleet server.
When Icinga notifies, the handler first asks the fleet server why
(`check_nsclient fleet explain`) and mails the answer with the alert. These
three scripts set that up on the same VM and estate, and run its last step as
a gate.

```powershell
./icinga/setup-icinga-machine.ps1                     # the same VM as the other flows
./provision-fleet-machines.ps1 -Windows 1 -Ubuntu 1 -WindowsPackageUrl <msi> -UbuntuPackageUrl <deb>
./icinga/setup-icinga-explained-alerts.ps1            # Icinga side: tutorial Steps 4-6, and mail
./icinga/configure-explained-agents.ps1               # agent side: tutorial Step 3
./icinga/verify-icinga-explained.ps1                  # Step 7: break a check, read the mail
```

What the tutorial's steps map to:

| Tutorial | Here |
|---|---|
| 1-2. A model and a fleet server | Taken as given: the fleet server the estate is enrolled with. Explanations must be switched on for the tenant (*Settings* in the fleet console), with any provider. |
| 3. Agent: passive checks, evidence, enrollment | `configure-explained-agents.ps1` writes the tutorial's `nsclient.ini` keys into each machine's own `nsclient.ini` over RunCommand and restarts the service, as the tutorial does by hand. Enrollment is the provisioning. |
| 4. Passive services in Icinga | `icinga/explained/nscp-passive.conf`, with one `Host` per machine of the manifest. |
| 5. `check_nsclient` and a key | Installed to `/usr/local/bin`; the key goes into `/etc/icinga2/fleet-explain.env`. The setup asks about a host that does not exist, as the daemon user, to prove the key and the trust store work without spending a model call. |
| 6. The notification handler | `icinga/explained/mail-service-explained.sh` and `notify-explained.conf`, both as the tutorial has them. |
| 7. Trigger a failure, read the mail | `verify-icinga-explained.ps1`. |

The gate breaks `disk_c` on every machine (`crit=free<99%`), waits for Icinga
to hold it as a hard CRITICAL and to have notified, and then reads the mail.
It passes when the mail has the *Why* section with a *Based on* block that
rests on the agent's own report of `disk_c`, and the check recovers with a
recovery mail once the threshold is back. It prints every explanation. A run
costs one model call per machine. `-BaselineOnly` stops before anything is
broken.

Things worth knowing:

- **Mail works on the VM without an account.** The setup installs postfix and
  `bsd-mailx`. By default `icingaadmin` is `alerts@localhost`, a mailbox on
  the VM (`/var/mail/alerts`), and a blind copy of *every* mail goes there
  too. That copy is what the gate reads, so the gate never depends on a mail
  server. Read it yourself with `./connect-machine.ps1 -PwdFile .icinga.pwd`
  and `sudo cat /var/mail/alerts`.
- **Real mail needs a relay.** Azure blocks outbound port 25, so mail to an
  address elsewhere goes through a submission server:

  ```powershell
  $env:NSCLIENT_SMTP_PASSWORD = "..."
  ./icinga/setup-icinga-explained-alerts.ps1 -MailTo you@example.com -MailFrom alerts@example.com `
      -SmtpHost smtp.example.com -SmtpUser alerts@example.com
  ```

  Port 587 with STARTTLS is the default; `-SmtpPort 465` switches to implicit
  TLS. `-MailFrom` rewrites the sender, which most relays insist on. The setup
  sends a test mail and prints whether the relay accepted it. Then break the
  check by hand and wait for the mail:

  ```powershell
  ./icinga/configure-explained-agents.ps1 -BreakDisk      # and -RestoreDisk afterwards
  ```
- **`fleet explain` needs check_nsclient 1.2.0 or later.** The setup takes
  the latest release from GitHub and refuses a build without the command;
  `-CheckNsclientUrl` names another build.
- **The key is stored on the VM.** Use a view_only key
  (`NSCLIENT_FLEET_SYNC_API_KEY`); the script falls back to the main key with
  a warning. `./icinga/setup-icinga-explained-alerts.ps1 -RemoveKey` takes it
  off again, after which notifications are mailed with *No explanation
  available*.
- **One flow per host.** The hosts are named like the ones the other two
  flows create. `-ReplaceHosts` takes them over; unassign the `icinga-passive`
  and `icinga-agent` bundles first, or the agents keep serving two masters.
  The stale `nagios-nrdp` bundle matters too: its schedules have the same
  names on another channel.
- **Where the configuration differs from the tutorial** is listed at the top
  of `icinga/explained/nscp-passive.conf` and of
  `configure-explained-agents.ps1`. The one that matters: Icinga 2 has no
  `check_freshness` / `freshness_threshold`, and refuses a service that sets
  them.
- **The settle time decides how good the explanation is.** Icinga notifies on
  the first failed result, while the agent's report of it reaches the fleet
  server on its next poll. `-ExplainSettle` (75 s by default) is how long the
  handler waits before it asks; the gate fails with "does not rest on the
  agent's report" when that was not enough.

## Connect to a running VM

`connect-machine.ps1` opens an interactive session to whatever the last setup
run left in `.vm.pwd` — Remote Desktop for a Windows VM, SSH for a Linux one —
picking the client and credentials automatically. It works from WSL too (it
finds `mstsc.exe` / `cmdkey.exe` under `/mnt/c/Windows/System32`).

```powershell
./connect-machine.ps1                 # connect to the current .vm.pwd target
./connect-machine.ps1 -PrintOnly      # just show IP / user / password
./connect-machine.ps1 -Os windows -PublicIp 20.1.2.3 -User azureadmin -Password 'PW'
```

Since `.vm.pwd` only describes the **last** machine provisioned, to debug a
specific VM either pass its details explicitly, or keep it alive when you
provision it — e.g. `./run-estate.ps1 base -Sequential -KeepVms`, which also
copies the credentials to `.vm.pwd` — then `./connect-machine.ps1` to RDP in.

## Run everything in one command

The [role-based estate](#role-based-estate-one-command-per-suite) is the
one-command path: provision, enroll, check, collect logs, tear down. The
`base` role is NSClient++ alone, and its machines round-robin over Windows,
Ubuntu and Rocky, so the old whole-OS matrix is one suite:

```powershell
# Windows Server 2025, Ubuntu, Rocky and Windows Server 2019, latest release
./run-estate.ps1 base=4 -WindowsVersions windows-2025,windows-2019

# A pinned release
./run-estate.ps1 base=3 -Version 0.24.1

# The last build of main, before it is released
./run-estate.ps1 base=3,scripts -FromMain

# The build of any other run, e.g. a pull request's (the id is in the run's url)
./run-estate.ps1 base=3 -BuildRunId 40172205677
```

`-FromMain` takes the packages of the newest successful *Build main* run (the
x64 MSI, the Ubuntu 24.04 deb and the Rocky 9 rpm, as far as the suites need
them). A VM cannot download a workflow artifact, which needs a GitHub token, so
`estate/publish-build.ps1` downloads them with the `gh` CLI, uploads them to a
private blob container in the `NSCP-Packages` resource group (created on first
use) and hands the VMs read-only SAS urls that expire after 12 hours. A run's
packages are uploaded once, and a second run of the same build reuses them.
It needs `gh auth login` and the `Az.Storage` module next to `Az.Resources`.
It prints a warning when main has moved past its newest successful build, so
you know the commit you are testing. `Remove-AzResourceGroup -Name NSCP-Packages`
removes the packages again. `./estate/publish-build.ps1` on its own returns
the urls, for `provision-fleet-machines.ps1 -WindowsPackageUrl ...`.

Every machine gets the role checks and the agent checks of the live suite
(`check_cpu`, `check_memory`, `check_uptime`, `check_drivesize`, plus the
Linux- or Windows-only ones), all in jest. The estate needs a fleet server
(see [Build a fleet](#build-a-fleet-machines-enrolled-with-a-fleet-server)).
Without one, provision a single machine with a setup script and run the live
suite against it ([below](#2-run-the-acceptance-suite)).

Supported `-WindowsVersions` values: `windows-2025`, `windows-2022`,
`windows-2019` (Windows Server), and `windows-11`, `windows-10` (client). Mix
them freely, e.g. test every Windows:

```powershell
./run-estate.ps1 base=5 -Version 0.14.0 -MaxMachines 5 `
  -WindowsVersions windows-2025,windows-2022,windows-2019,windows-11,windows-10
```

> **Client Windows (10/11) needs an eligible subscription.** They deploy as
> Trusted Launch images, and Azure only allows Windows client images on
> subscriptions with client rights (Visual Studio / Enterprise Dev-Test, or
> multi-session via AVD). On a plain pay-as-you-go subscription the deploy is
> rejected — the Server SKUs have no such restriction.

## 1. Provision + install

Each setup script creates a resource group, VM, network + NSG (opens 8443 /
5666 and RDP/SSH/WinRM), installs the requested NSClient++ **release**, runs
`nscp web install --https --allowed-hosts '0.0.0.0/0,::/0' --password <random>`, opens the
firewall, and saves the VM's address + web password to `.vm.pwd` in this
directory.

```powershell
# Windows
./win/setup-machine.ps1        -VmName NSCP-Test        -Version 0.14.0 -WindowsVersion windows-2025

# Ubuntu
./linux/setup-ubuntu-machine.ps1 -VmName NSCP-Ubuntu-Test -Version 0.14.0 -UbuntuVersion 24.04

# Rocky
./linux/setup-rocky-machine.ps1  -VmName NSCP-Rocky-Test  -Version 0.14.0 -RockyVersion 9
```

## 2. Run the acceptance suite

The Jest **live** suite ([`tests/live`](../../tests/live)) reads the VM's
address and web password from the credentials file the setup script wrote.
With no `NSCP_TARGET_PASSWORD` set it uses `build/powershell/.vm.pwd`, the
machine provisioned last, so from the repository root:

```powershell
npm --prefix tests run test:live -- live-checks
```

Another machine's file goes in `NSCP_TARGET_PWD_FILE` (the setup script
prints the full line when it finishes):

```powershell
$env:NSCP_TARGET_PWD_FILE = 'build/powershell/.vm.web-ams-04.pwd'; npm --prefix tests run test:live -- live-checks
```

The suite connects over REST (HTTPS/8443, self-signed cert accepted), logs in,
and asserts the standard checks work on the real machine: cross-platform
(`check_cpu`, `check_memory`, `check_uptime`, `check_drivesize`) plus
platform-specific ones (Linux: `check_load`, `check_swap_io`, `check_service`,
`check_mount`; Windows: `check_service`, `check_eventlog`). The estate runs the
same cases on every machine.

It also runs **locally without a VM** for debugging: set `NSCP_TARGET_URL` and
`NSCP_TARGET_PASSWORD` instead (see
[`tests/live/README.md`](../../tests/live/README.md)).

## 3. Logs / teardown

```powershell
./win/show-log.ps1   -VmName NSCP-Test          # or linux/show-log.ps1
./teardown-machine.ps1 -ResourceGroupName NSCP-RG
```

## Notes

- `.vm.pwd` holds a live web password — it is intentionally not committed
  (add it to your local ignore if needed) and is overwritten by each setup run.
- One `.vm.pwd` at a time: provision, test, tear down before the next VM, or
  pass `-PwdFile .vm.<name>.pwd` to the setup script and `NSCP_TARGET_PWD_FILE`
  to the suite for parallel VMs.
- A GitHub Actions pipeline that wraps `run-estate.ps1` and always tears down
  is the natural next step.
