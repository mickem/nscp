# Plan: role-based fleet estate tests

An acceptance lab where machines carry a *role* (a workload such as SQL Server
or MariaDB next to NSClient++), every machine enrolls with a fleet server, the
fleet server provisions the role's checks through a bundle, and a gate then
verifies from the outside that each machine's checks return real results.

Everything below builds on the tooling in this directory. The estate scripts,
the per-machine credentials files, the Nagios bundle composer and the
`tests/live` suite stay as they are; the plan adds a role catalog on top and
generalises two of the scripts.

## Goals

1. **One command per suite.** `./run-estate.ps1 mysql` provisions the one
   machine the `mysql` role needs, composes its bundle, verifies it, collects
   its logs and tears it down.
2. **Several suites at once, bounded by quota.** `./run-estate.ps1 mysql,mssql,iis
   -Parallel` runs them concurrently, and `-MaxMachines` (default 5) caps the
   estate so a run never trips the vCPU quota half way through. `all` is an
   explicit opt-in and warns when it exceeds the cap instead of failing ten
   minutes in.
3. **Failures are diagnosable from the runner.** Every run leaves a
   `.estate-runs/<timestamp>/<machine>/` folder with the agent log, the role
   install log, the fleet sync state and the raw check output, whether it
   passed or not. `-KeepVms` keeps the failed machines up for a look inside.
4. **Roles are additive.** A new role is one folder: a JSON descriptor, an
   install fragment per OS it supports and a list of checks with their expected
   outcome. Nothing in the runner changes.

## Layout

```
build/powershell/
  estate/
    roles/
      base/          role.json                       # NSClient++ only (today's estate)
      mysql/         role.json  install-ubuntu.sh  install-rocky.sh
      mssql/         role.json  install-windows.ps1
      iis/           role.json  install-windows.ps1
      docker/        role.json  install-ubuntu.sh
      ...
    estate-api.ps1        # role catalog reader, quota budget, run folder helpers
    add-role-bundles.ps1  # one group + one bundle per role (generalised add-nagios-bundle)
    verify-estate.ps1     # active gate: remote checks + in-sync state
    collect-logs.ps1      # pull logs from every machine into the run folder
  run-estate.ps1          # provision -> bundles -> verify -> collect -> teardown
tests/live/
  estate.test.ts          # role-aware live suite, reads the manifest
```

### `role.json`

```json
{
  "name": "mysql",
  "description": "MariaDB server on Linux, checked with CheckMySQL",
  "os": ["ubuntu", "rocky"],
  "vcpus": 1,
  "vmSize": null,
  "image": null,
  "modules": ["CheckMySQL"],
  "install": { "ubuntu": "install-ubuntu.sh", "rocky": "install-rocky.sh" },
  "installTimeoutMinutes": 10,
  "tag": { "role": "mysql" },
  "bundle": {
    "settings": {
      "mysql": { "default": { "host": "127.0.0.1", "user": "nscp", "password": "${ROLE_PASSWORD}" } }
    }
  },
  "checks": [
    { "command": "check_mysql",         "args": {},                      "expect": "OK", "perf": ["uptime"] },
    { "command": "check_mysql_query",   "args": { "query": "SELECT 1" }, "expect": "OK" }
  ],
  "logs": {
    "ubuntu": ["/var/log/nsclient/nsclient.log", "/var/log/mysql/error.log", "/var/log/nscp-role-install.log"]
  }
}
```

Field notes:

- `modules` are activated **at install time** by the setup script. A bundle
  writes the module into `fleet.ini` but cannot load it into a running agent
  (see the Nagios section of README.md); activating it during install is what
  makes the bundle take effect on the first sync. The bundle enables the same
  modules anyway so it also works on a host enrolled some other way.
- `image` and `vmSize` override the family defaults, for roles that need a
  Marketplace image (SQL Server Express) or more than one vCPU (Hyper-V).
  `vcpus` is what the quota budget counts.
- `${ROLE_PASSWORD}` is generated per machine, handed to the install fragment
  as an environment variable, written to `.vm.<name>.pwd`, and substituted into
  the bundle. It is a throwaway secret on a throwaway estate and the README
  says so.
- `checks` are what the gate runs against the machine over REST. `expect` is a
  Nagios state or `"any"`; `perf` lists perfdata keys that must be present.
  Thresholds are not pinned here: the gate is shape-and-healthy, like
  `tests/live`, and the forced WARNING/CRITICAL cases stay in the
  self-spawning suites.

### Provisioning

`provision-fleet-machines.ps1` gains `-Roles` (`mysql=1,mssql=2`), keeps
`-Windows/-Ubuntu/-Rocky` as shorthand for `base=N` per family, and for each
machine:

1. picks the OS from the role's `os` list (round-robin when several),
2. names the machine `<role>-<site>-<nn>` from the role name rather than at
   random (the name generator already supports role prefixes),
3. mints the bootstrap token and calls the family setup script with two new
   parameters: `-Modules` (the role's list, appended to the standard ones) and
   `-RoleInstallScript` (the fragment, run through the existing RunCommand
   helper *before* NSClient++ is installed, so a check can be verified as soon
   as the agent is up, with its output captured to
   `/var/log/nscp-role-install.log` or `C:\nscp-role-install.log`),
4. sets the role tag on the fleet host (`PUT /api/hosts/<id>/tags`, the same
   operator-set tag the live fleet test uses for group selection),
5. records role and password in the manifest entry.

The quota preflight that already exists sums `vcpus` over the requested roles
and refuses up front when the estate plus the machines still in the manifest
exceed `-MaxMachines` or the regional quota.

### Bundles

`add-role-bundles.ps1` is `add-nagios-bundle.ps1` with the catalog swapped:
for every role present in the manifest it ensures a group `role-<name>` with
selector `{ clauses: [{ op: "eq", key: "role", value: "<name>" }] }`, composes
bundle `role-<name>` from the role's `modules` and `bundle.settings` with the
password substituted, and assigns it. Re-running composes a new version and
moves the assignment, as the Nagios script does today. The Nagios bundle can
still be layered on the same estate; the two do not overlap.

### Verification

`verify-estate.ps1` polls, up to `-WaitMinutes`, until for every machine in
the manifest:

1. the fleet host reports enrolled and in sync with the role bundle
   (`GET /api/hosts`, the field `verify-nagios-estate.ps1` already reads),
2. every role check returns its expected state with its perfdata over REST.

It then runs `tests/live/estate.test.ts` for the jest-shaped record. That
suite reads the manifest, logs in to each machine with `live-target.ts` and
runs the role's `checks`; it is what a developer runs by hand against a kept
estate (`npm run test:live -- estate`) while iterating on a check.

Two optional layers on the same estate, each behind a switch:

- `-CheckNsclient`: run the `check_nsclient` binary from the runner against
  every machine for each check, so the plugin operators actually use is
  exercised too.
- a `hub` role: a Linux machine with `CheckNet`, `NRPEClient` and `NSCPClient`
  whose checks target the *other* machines (`check_nsclient_web_online`,
  `check_nrpe`, `check_remote_nscp`). This is the one place the remote-check
  modules get a real multi-host test.

### Logs

`collect-logs.ps1` writes `.estate-runs/<timestamp>/<machine>/` with:

| File | Source |
| ---- | ------ |
| `nsclient.log` | the agent log (the existing `show-log.ps1` chunked fetch, made a function) |
| `role-install.log` | the install fragment's captured output |
| `fleet.ini`, `agent-state.json` | what the bundle rendered and where the sync stands |
| `fleet-host.json` | `GET /api/hosts/<id>` at collection time |
| `checks.json` | raw REST result of every role check, last attempt |
| `journal.txt` / `events.txt` | `journalctl -u nsclient` or the Application event log, last 500 lines |

`run-estate.ps1` collects **before** teardown, on success and failure alike,
and prints the run folder and a one-line per-machine verdict at the end. On a
failure the verdict names the first failing stage (install, enroll, sync,
check) so the right log is the first one opened.

### Runner

```powershell
./run-estate.ps1 mysql                          # one suite, sequential
./run-estate.ps1 mysql,mssql,iis -Parallel      # three, concurrently
./run-estate.ps1 all -Parallel -MaxMachines 8   # everything the quota allows
./run-estate.ps1 mssql -KeepVms                 # leave it up, then connect-machine.ps1
./run-estate.ps1 -Collect                       # just pull logs from the current manifest
```

Sequential runs share `.vm.pwd`; `-Parallel` uses `.vm.<name>.pwd` per machine
and `-MaxParallel`, as `run-all-tests.ps1` does. Exit code is non-zero if any
suite failed; `-StopOnFirstFailure` and `-Version`/`-*PackageUrl` carry over.

## Phases

| Phase | Deliverable | Size |
| ----- | ----------- | ---- |
| 1 | Role catalog + `base` role, `-Roles` in provisioning, `-Modules`/`-RoleInstallScript` in the setup scripts, quota budget, `collect-logs.ps1`, `run-estate.ps1` | the framework, ~1.5 days |
| 2 | `mysql` role (Ubuntu + Rocky), `add-role-bundles.ps1`, `verify-estate.ps1`, `estate.test.ts` | first end-to-end green run, ~1 day |
| 3 | `mssql` role via the Marketplace SQL Server 2022 Express image, `msodbcsql18` in the fragment | ~1 day, mostly waiting on Azure |
| 4 | `hub` role and `-CheckNsclient` | ~0.5 day |
| 5 | further roles, one at a time (below) | 0.5 to 1 day each |

Phase 1 is testable on its own: `run-estate.ps1 base` is today's estate with
log collection. Phase 2 proves the whole pipeline on the cheapest workload
before the slow SQL Server install enters the picture.

## Roles to add down the line

Ordered by value per effort. "Fits" means a single Azure VM of the default
size can host it.

| Role | OS | Modules / checks exercised | Workload install | Fits |
| ---- | -- | -------------------------- | ---------------- | ---- |
| `mysql` | Ubuntu, Rocky | CheckMySQL: `check_mysql`, `check_mysql_query` | `apt/dnf install mariadb-server`, minutes | yes |
| `mssql` | Windows | CheckMSSQL: all five commands | Marketplace *SQL Server 2022 Express on Windows Server 2022* image; enable TCP 1433 + `sa` | yes |
| `iis` | Windows | CheckWindowsApps: `check_iis_sites`, `check_iis_app_pools`, `check_iis_worker_processes`, `check_iis_request_queues` | `Install-WindowsFeature Web-Server`, one site + pool, minutes | yes |
| `docker` | Ubuntu | CheckDocker: `check_docker`, `check_docker_info`, `check_docker_stats`, `check_docker_restarts`, `check_docker_df` | docker.io + one always-restarting container | yes |
| `webstatus` | Ubuntu | CheckNet: `check_apache_status`, `check_nginx_status`, `check_phpfpm_status`, `check_tomcat_status`, `check_http`, `check_tcp`, `check_ssh` | the `tests/Dockerfiles/*-status` images already say how each is configured | yes |
| `hub` | Ubuntu | CheckNet `check_nsclient_web_online`, NRPEClient `check_nrpe`, NSCPClient `check_remote_nscp`, NRPEServer/NSClientServer on the targets | none; checks point at the other machines | yes |
| `security` | Windows | CheckSecurity: `check_firewall`, `check_defender`, `check_local_accounts`, `check_group_members`, `check_certificate`, `check_activation`, `check_nla` | none beyond a test cert and a test group; deterministic on a fresh server | yes |
| `tasksched` / `eventlog` | Windows | CheckTaskSched, CheckEventLog with a known task and a written event | a scheduled task and `Write-EventLog`, seconds | yes (could fold into `security`) |
| `kubernetes` | Ubuntu | CheckKubernetes: `check_kubernetes`, `check_pods`, `check_nodes`, `check_workloads` | k3s single node, ~2 minutes, 2 vCPU recommended | yes, 2 vCPU |
| `radius` | Ubuntu | CheckNet `check_radius` against FreeRADIUS | `freeradius` package, the docker fixture has the config | yes |
| `nps` | Windows | CheckWindowsApps `check_nps_*` | NPS role + policy; `tests/live/nps.md` is the manual lab today | yes, more setup |
| `ad` | Windows | CheckActiveDirectory: `check_ad_replication`, `check_secure_channel`, `check_kdc` | `Install-ADDSForest`, a reboot, ~15 minutes; replication needs a second DC | yes, slow; 2 machines for replication |
| `rds` | Windows | CheckWindowsApps `check_rds_*` | RDS roles; licensing and broker need a domain | with `ad`, heavy |
| `cluster` | Windows | CheckWindowsApps `check_cluster_*` | two-node failover cluster, shared storage, domain | two machines + `ad`; `tests/live/cluster.md` |
| `hyperv` | Windows | CheckHyperV | needs nested virtualization (Dv3/Ev3 or newer, 2+ vCPU) and a VM inside | 2+ vCPU, image restrictions |
| `macos` | macOS | CheckSystemUnix on Darwin | no Azure image; would need a different provider | no |

Recommended order after the plan's phases: `iis`, `docker`, `security`,
`webstatus`, `kubernetes`, `radius`. `nps`, `ad`, `rds` and `cluster` form a
domain family that shares a domain-controller role and is worth its own plan.

## Quota and cost

The default size is 1 vCPU, so `-MaxMachines` and the regional quota (10
vCPUs on a fresh subscription) are the same number until a role asks for more.
A full run of the recommended roles is 8 machines; `all` including the domain
family is 12 to 14 and needs two regions (`-Location` is per machine already)
or a raised quota. A single-role run is 20 to 40 minutes wall-clock, dominated
by Azure create and destroy; the SQL Express image adds nothing, a silent SQL
install would add 15.

## Out of scope

- CI. This stays hand-run tooling like the rest of the directory. A
  `workflow_dispatch` workflow in the shape of `manual-scenarios.yml` can wrap
  `run-estate.ps1` later once Azure credentials live in repository secrets.
- Forced WARNING/CRITICAL threshold cases; the self-spawning `tests/` suites
  own those.
- Mutating workloads during the run (stopping SQL Server to see CRITICAL).
  Worth a later `-Chaos` switch on the gate, not part of this.
