---
icon: "🖥️"
modules: [CheckExternalScripts]
action: none
---
**New sample script: Hyper-V hosts as SCVMM sees them.**
`scripts\check_scvmm_hosts.ps1` checks every host System Center Virtual
Machine Manager manages: CRITICAL when VMM cannot communicate with a host or
its overall state is not OK, WARNING for the states given in `-Warning`
(by default `NeedsAttention,Updating`); a host in a state given in `-Ignore`
(by default `MaintenanceMode`) is not reported. It needs the VMM console's
PowerShell module, so it runs where the console is installed (usually the
VMM server). It is shipped but not configured: add it under
`[/settings/external scripts/scripts]` to use it, e.g.
`check_scvmm_hosts = powershell.exe -NoProfile -ExecutionPolicy Bypass -NonInteractive -File scripts\check_scvmm_hosts.ps1 -Server vmm01`.
