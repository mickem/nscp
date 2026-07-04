# Azure test-machine scripts

Hand-run PowerShell helpers for spinning up a throwaway Azure VM, installing a
released NSClient++ on it, running the acceptance suite against it, and tearing
it down again. These are **manual** tooling for personal/release verification —
not a CI pipeline (that can be layered on top later).

> Requires the Az PowerShell modules. Install them once with
> `./install-azure.ps1`, then connect with `./connect-to-azure.ps1`
> (or the setup scripts will call `Connect-AzAccount` themselves).

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
                 show-log.ps1         →  pull nsclient.log / install log
                          │
              teardown-machine.ps1    →  delete the resource group
```

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
