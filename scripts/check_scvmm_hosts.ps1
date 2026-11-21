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
# Usage - a plain script entry that calls powershell.exe with -File, so the
# script's exit code reaches the agent and the arguments stay with the entry:
#   [/settings/external scripts/scripts]
#   check_scvmm_hosts = powershell.exe -NoProfile -ExecutionPolicy Bypass -NonInteractive -File scripts\check_scvmm_hosts.ps1 -Server vmm01 -HostGroup "All Hosts\Production"
#
# (An entry under [wrapped scripts] works too, but the bundled ps1 wrapping
# only forwards the arguments the caller sends, not ones written in the entry.)
#
# Parameters:
#   -Server    VMM server to connect to (default: localhost)
#   -HostGroup Only check hosts in this host group path and the groups below
#              it (default: all)
#   -VMHost    Only check hosts whose name matches this wildcard (default: *)
#   -Warning   Overall states that warn (default: NeedsAttention,Updating)
#   -Ignore    Overall states that are not reported (default: MaintenanceMode)
#
# -Warning and -Ignore take a comma separated list: powershell.exe -File hands
# "A,B" over as one string, so the script splits it itself.
#
# A host in an -Ignore state is skipped, reachable or not: it is out of
# service on purpose. Any other host is CRITICAL when VMM cannot talk to it
# (CommunicationState other than Responding) or its OverallState is anything
# but OK or a -Warning state; WARNING for a -Warning state. No VMM module, no
# server or no hosts matched is UNKNOWN. Exit codes follow the Nagios plugin
# convention.

param(
    [string]$Server = "localhost",
    [string]$HostGroup = "",
    [string]$VMHost = "*",
    [string[]]$Warning = @("NeedsAttention", "Updating"),
    [string[]]$Ignore = @("MaintenanceMode")
)

# powershell.exe -File does not parse "A,B" as an array.
function Split-StateList([string[]]$values) {
    @($values | ForEach-Object { "$_" -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}
$Warning = Split-StateList $Warning
$Ignore = Split-StateList $Ignore

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
        # The group itself and every group below it. Whether Get-SCVMHost
        # -VMHostGroup descends on its own is not relied upon: each group is
        # asked and the hosts are de-duplicated by name.
        $root = $HostGroup.TrimEnd('\')
        $groups = @(Get-SCVMHostGroup -VMMServer $vmm -ErrorAction Stop | Where-Object {
            $path = "$($_.Path)".TrimEnd('\')
            $path -eq $root -or $path.StartsWith("$root\", [System.StringComparison]::OrdinalIgnoreCase)
        })
        if ($groups.Count -eq 0) {
            Write-Output "UNKNOWN: host group '$HostGroup' not found on $Server"
            exit $UNKNOWN
        }
        $seen = @{}
        $hosts = @()
        foreach ($group in $groups) {
            foreach ($candidate in @(Get-SCVMHost -VMHostGroup $group -ErrorAction Stop)) {
                if (-not $seen.ContainsKey("$($candidate.Name)")) {
                    $seen["$($candidate.Name)"] = $true
                    $hosts += $candidate
                }
            }
        }
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
    # The name ends up in the message, where a | would start the perfdata.
    $name = "$($h.Name)" -replace '\|', '_'
    $overall = "$($h.OverallState)"
    $communication = "$($h.CommunicationState)"
    if ($communication -eq "Responding") { $responding++ }
    if ($Ignore -contains $overall) {
        # Deliberately out of service (maintenance mode): not a problem,
        # reachable or not.
        continue
    }
    if ($communication -ne "Responding") {
        $problems += "${name}: $communication ($overall)"
        $status = $CRIT
    } elseif ($overall -eq "OK") {
        continue
    } elseif ($Warning -contains $overall) {
        $problems += "${name}: $overall"
        if ($status -lt $WARN) { $status = $WARN }
    } else {
        $problems += "${name}: $overall"
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
