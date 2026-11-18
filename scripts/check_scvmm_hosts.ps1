# SPDX-FileCopyrightText: 2004-2026 Michael Medin
# SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#
# check_scvmm_hosts.ps1 - Hyper-V hosts as System Center Virtual Machine
# Manager sees them.
#
# A sample for CheckExternalScripts, not a native check: SCVMM is only
# reachable through its PowerShell module (VirtualMachineManager, installed
# with the VMM console), so this runs wherever that console is installed -
# usually the VMM server itself. CheckHyperV covers the Hyper-V host the agent
# runs on; this covers the fabric view: every host VMM manages, whether VMM can
# reach it and whether VMM considers it healthy.
#
# Usage (from the bundled `ps1` wrapping):
#   [/settings/external scripts/scripts]
#   check_scvmm_hosts = check_scvmm_hosts.ps1 -Server vmm01 -HostGroup "All Hosts\Production"
#
# Parameters:
#   -Server     VMM server to connect to (default: localhost)
#   -HostGroup  Only check hosts under this host group path (default: all)
#   -VMHost     Only check hosts whose name matches this wildcard (default: *)
#   -Warning    Overall states that warn (default: NeedsAttention, Updating)
#
# A host is CRITICAL when VMM cannot talk to it (CommunicationState other
# than Responding) or its OverallState is anything but OK or a -Warning
# state; WARNING for a -Warning state. No VMM module, no server or no hosts
# matched is UNKNOWN. Exit codes follow the Nagios plugin convention.

param(
    [string]$Server = "localhost",
    [string]$HostGroup = "",
    [string]$VMHost = "*",
    [string[]]$Warning = @("NeedsAttention", "Updating")
)

$OK = 0; $WARN = 1; $CRIT = 2; $UNKNOWN = 3

try {
    Import-Module VirtualMachineManager -ErrorAction Stop
} catch {
    Write-Output "UNKNOWN: the VirtualMachineManager PowerShell module is not installed (install the VMM console): $($_.Exception.Message)"
    exit $UNKNOWN
}

try {
    $vmm = Get-SCVMMServer -ComputerName $Server -ErrorAction Stop
} catch {
    Write-Output "UNKNOWN: cannot connect to VMM server ${Server}: $($_.Exception.Message)"
    exit $UNKNOWN
}

try {
    if ($HostGroup) {
        $group = Get-SCVMHostGroup -VMMServer $vmm -ErrorAction Stop | Where-Object { $_.Path -eq $HostGroup }
        if (-not $group) {
            Write-Output "UNKNOWN: host group '$HostGroup' not found on $Server"
            exit $UNKNOWN
        }
        $hosts = @(Get-SCVMHost -VMHostGroup $group -ErrorAction Stop)
    } else {
        $hosts = @(Get-SCVMHost -VMMServer $vmm -ErrorAction Stop)
    }
} catch {
    Write-Output "UNKNOWN: failed to list hosts on ${Server}: $($_.Exception.Message)"
    exit $UNKNOWN
}

$hosts = @($hosts | Where-Object { $_.Name -like $VMHost })
if ($hosts.Count -eq 0) {
    Write-Output "UNKNOWN: no hosts matched on $Server"
    exit $UNKNOWN
}

$status = $OK
$problems = @()
$responding = 0
foreach ($h in $hosts) {
    $overall = "$($h.OverallState)"
    $communication = "$($h.CommunicationState)"
    if ($communication -eq "Responding") { $responding++ }
    if ($communication -ne "Responding") {
        $problems += "$($h.Name): $communication"
        $status = $CRIT
    } elseif ($overall -ne "OK" -and $Warning -contains $overall) {
        $problems += "$($h.Name): $overall"
        if ($status -lt $WARN) { $status = $WARN }
    } elseif ($overall -ne "OK") {
        $problems += "$($h.Name): $overall"
        $status = $CRIT
    }
}

$prefix = @("OK", "WARNING", "CRITICAL", "UNKNOWN")[$status]
$perf = "'hosts'=$($hosts.Count);;;0 'responding'=$responding;;;0 'problems'=$($problems.Count);;;0"
if ($problems.Count -eq 0) {
    Write-Output "${prefix}: all $($hosts.Count) host(s) OK on $Server|$perf"
} else {
    Write-Output "${prefix}: $($problems -join ', ')|$perf"
}
exit $status
