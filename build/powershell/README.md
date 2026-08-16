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

```powershell
$env:NSCLIENT_FLEET_API_KEY = "nsk_..."      # API keys, in the fleet sidebar
./provision-fleet-machines.ps1 -FleetServer https://fleet.example.com

# A bigger, mixed estate, provisioned concurrently:
./provision-fleet-machines.ps1 -FleetServer https://fleet.example.com `
    -Windows 3 -Ubuntu 2 -Rocky 1 -Parallel

# See what it would create, without touching Azure or the fleet server:
./provision-fleet-machines.ps1 -DryRun -Windows 2 -Ubuntu 2
```

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

> **Linux enrollment material has to be readable by the service user.** `nscp
> enroll` runs under `sudo`, so `agent-state.json` lands root-owned and `0600`,
> while the DEB/RPM run the service as `nsclient`. The core's boot gate is a
> plain existence check, so it starts the sync, fails to read the manifest, and
> the sync thread dies — the machine installs, enrolls, reports success, and
> never appears in the fleet. The Linux setup scripts now hand the manifest and
> the managed directory (`<shared-path>/fleet`) to the service user and assert
> both are usable *as that user* before reporting success. Windows is unaffected
> (the service runs as LocalSystem).
>
> Note this failure is invisible on the box: the packaged log file is root-owned
> too, so the service cannot write it (`Failed to create log directory`). Use
> `journalctl -u nsclient | grep -i fleet` when debugging enrollment.

Every run appends to `.fleet-machines.json` (name, OS, resource group, public IP,
fleet host id). Tear the whole estate down again with:

```powershell
./provision-fleet-machines.ps1 -Destroy -FleetServer https://fleet.example.com -RemoveFleetHosts
```

`-RemoveFleetHosts` also deletes the hosts from the fleet server, so a torn-down
VM does not linger there as permanently offline. That needs an admin/owner API
key (an `add_hosts` key may create hosts but not delete them); it is skipped with
a warning if the key is not allowed, and the Azure teardown still runs.

Useful options: `-Version` (release to install, default 0.15.0),
`-WindowsPackageUrl` / `-UbuntuPackageUrl` / `-RockyPackageUrl` to install a
build from your own url instead — needed to exercise the installer's own fleet
enrollment before it ships in a release — `-WindowsVersions` (round-robined over
the Windows machines), `-Location`, `-MaxParallel`, and `-ResourceGroupPrefix`.

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
