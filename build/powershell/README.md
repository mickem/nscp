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
                 run-tests.ps1        →  live acceptance suite (tests/live)
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

Useful options: `-Version` (release to install, default 0.17.0; Linux machines
need 0.16.2 or newer - the script refuses older releases up front, see above),
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
> activate `NRDPClient` and `Scheduler` at install time, so a machine they
> provision is ready for the Nagios bundle whenever it arrives. **A host enrolled
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
> release is deliberately the boring choice; an Icinga 2 variant of the same
> flow (its REST API replacing the cfg-file rendering) is the planned next
> step.

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
provision it — e.g. `./run-all-tests.ps1 -Target windows -WindowsVersions windows-2025 -KeepVms`,
then `./connect-machine.ps1` to RDP in.

## Run everything in one command

`run-all-tests.ps1` chains the three steps above (provision → test → teardown)
for one or several machine types. Each machine gets its own resource group and
is torn down when it finishes.

```powershell
# The whole matrix: Windows Server (latest + oldest) + Ubuntu + Rocky
./run-all-tests.ps1 -Target all -Version 0.14.0

# Same, but concurrently — much faster (most of the time is Azure create/destroy)
./run-all-tests.ps1 -Target all -Version 0.14.0 -Parallel

# A single family
./run-all-tests.ps1 -Target ubuntu  -Version 0.14.0
./run-all-tests.ps1 -Target windows -Version 0.14.0     # windows-2025 + windows-2019
./run-all-tests.ps1 -Target rocky   -Version 0.14.0     # 'redhat' is an alias
```

**Sequential vs. `-Parallel`.** By default machines run one at a time and share
`build/powershell/.vm.pwd` (so `connect-machine.ps1` and a bare `run-tests`
still find the last one). `-Parallel` (PowerShell 7+) runs them concurrently,
each with its own `.vm.<name>.pwd`, capped by `-MaxParallel` (default 4 — mind
your Azure vCPU quota). It logs in once up front so the concurrent setups don't
each prompt. Output from the machines interleaves (every line is prefixed with
the machine label); the pass/fail summary at the end is what matters. With
`-Parallel -KeepVms`, point at a specific box with
`./connect-machine.ps1 -PwdFile .vm.<name>.pwd`.

| `-Target`           | Machines provisioned                                  |
| ------------------- | ----------------------------------------------------- |
| `all`               | Windows 2025 + Windows 2019 + Ubuntu 24.04 + Rocky 9  |
| `windows`           | `-WindowsVersions` (default latest + oldest server)   |
| `ubuntu`            | `-UbuntuVersion` (default 24.04)                      |
| `rocky` / `redhat`  | `-RockyVersion` (default 9)                           |

It prints a pass/fail summary at the end and exits non-zero if any machine
failed. Useful switches: `-KeepVms` (skip teardown for debugging — only the last
machine's `.vm.pwd` survives), `-StopOnFirstFailure`, and `-WindowsVersions` /
`-UbuntuVersion` / `-RockyVersion` to override the images (e.g.
`-WindowsVersions windows-2025` for just the latest). The first machine performs
the Azure login (device-code on WSL); run `./install-azure.ps1` once beforehand
if the Az modules are missing.

Supported `-WindowsVersions` values: `windows-2025`, `windows-2022`,
`windows-2019` (Windows Server), and `windows-11`, `windows-10` (client). Mix
them freely, e.g. test every Windows:

```powershell
./run-all-tests.ps1 -Target windows -Version 0.14.0 `
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

[`run-tests.ps1`](run-tests.ps1) points the Jest **live** suite
([`tests/live`](../../tests/live)) at the VM. It reads the public IP + web
password from `.vm.pwd` (falling back to explicit `-PublicIp`/`-Password`, or an
Azure lookup for the IP), sets the `NSCP_TARGET_*` environment, and runs
`npm run test:live` in `tests/`.

```powershell
./run-tests.ps1 -VmName NSCP-Ubuntu-Test -Os linux
./run-tests.ps1 -VmName NSCP-Test        -Os windows
```

The suite connects over REST (HTTPS/8443, self-signed cert accepted), logs in,
and asserts the standard checks work on the real machine — cross-platform
(`check_cpu`, `check_memory`, `check_uptime`, `check_drivesize`) plus
platform-specific ones (Linux: `check_load`, `check_swap_io`, `check_service`,
`check_mount`; Windows: `check_service`, `check_eventlog`).

It also runs **locally without a VM** for debugging — see
[`tests/live/README.md`](../../tests/live/README.md):

```powershell
./run-tests.ps1 -Local -Password <web-pw> -Os linux
```

## 3. Logs / teardown

```powershell
./win/show-log.ps1   -VmName NSCP-Test          # or linux/show-log.ps1
./teardown-machine.ps1 -ResourceGroupName NSCP-RG
```

## Notes

- `.vm.pwd` holds a live web password — it is intentionally not committed
  (add it to your local ignore if needed) and is overwritten by each setup run.
- One `.vm.pwd` at a time: provision, test, tear down before the next VM, or
  pass `-PublicIp`/`-Password` to `run-tests.ps1` explicitly for parallel VMs.
- A GitHub Actions pipeline that downloads a release's artifacts, calls these
  scripts, runs `run-tests.ps1`, and always tears down is the natural next step.
