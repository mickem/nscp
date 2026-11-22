<#
.SYNOPSIS
    Configure the estate's agents for the "Icinga alert emails that explain
    themselves" tutorial: passive checks to Icinga, evidence with every failed
    check, failure context to the fleet server.
.DESCRIPTION
    Step 3 of the tutorial, on every machine of the estate manifest. The
    tutorial edits nsclient.ini and restarts the service, and so does this:
    the keys are written into the machine's own nsclient.ini over RunCommand
    (not delivered as a fleet bundle), each one is read back, and the service
    is restarted. The machines are enrolled already
    (provision-fleet-machines.ps1), which is the tutorial's "Enroll with the
    fleet server".

    What is written is the tutorial's nsclient.ini, with these differences:

      * `hostname` is ${host_lc}, not ${host_lc}.${domain_lc}: the machines
        have no domain, and the fleet server knows them by the bare name too.
      * The API user and password are those of the Icinga VM
        (setup-icinga-explained-alerts.ps1), and `ca` points at its CA, which
        this script copies to the machine.
      * The address is https://<the VM's Icinga node name>:5665/, and the
        machine's hosts file gets a line for that name. With `verify mode =
        peer-cert` the agent checks the name in the certificate, which Icinga
        issues for its node name - the estate has no DNS, and by IP the agent
        refuses the connection ("certificate verify failed").
      * The schedules come from explained-checks.json: the tutorial's, with a
        disk check for / on Linux, a shorter interval, and the thresholds
        written without quotes.
      * Both the Windows and the Linux diagnostics sections are written on
        each machine; a section the platform does not read is ignored.

    Run setup-icinga-explained-alerts.ps1 first: it creates the API user and
    saves the CA this script hands out.
.PARAMETER BreakDisk
    Step 7: write only the fault schedule, with the threshold that makes it
    fail (the catalog's "broken" command), and restart.
.PARAMETER RestoreDisk
    Put the fault schedule's normal command back, and restart.
.PARAMETER VerifyMode
    `verify mode` of the Icinga target (default peer-cert, as in the
    tutorial).
.PARAMETER PwdFile
    Credentials file of the Icinga VM (default: ../.icinga.pwd).
.PARAMETER ManifestFile
    The estate manifest (default: ../.fleet-machines.json).
.EXAMPLE
    ./configure-explained-agents.ps1
.EXAMPLE
    ./configure-explained-agents.ps1 -BreakDisk     # and -RestoreDisk afterwards
#>
param(
    [switch]$BreakDisk,
    [switch]$RestoreDisk,
    [string]$VerifyMode = "peer-cert",
    [string]$PwdFile = "",
    [string]$ManifestFile = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")
. (Join-Path $parentDir "estate/estate-api.ps1")

if ($BreakDisk -and $RestoreDisk) {
    Write-Error "❌ -BreakDisk and -RestoreDisk exclude each other."
    exit 1
}
if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".icinga.pwd" }
if (-not $ManifestFile) { $ManifestFile = Join-Path $parentDir ".fleet-machines.json" }
$icinga = Read-IcingaPwdFile -Path $PwdFile
if (-not $icinga.PublicIp -or -not $icinga.NodeName -or -not $icinga.ExplainUser -or -not $icinga.ExplainPassword) {
    Write-Error "❌ '$PwdFile' has no explained-alerts API user - run setup-icinga-explained-alerts.ps1 first."
    exit 1
}
$caFile = Join-Path (Split-Path -Parent $PwdFile) ".icinga-ca.crt"
if (-not (Test-Path $caFile)) {
    Write-Error "❌ '$caFile' does not exist - run setup-icinga-explained-alerts.ps1 first."
    exit 1
}
$caPem = ((Get-Content -Path $caFile) -join "`n").Trim()
$icingaAddress = "https://$($icinga.NodeName):5665/"
$hostsLine = "$($icinga.PublicIp) $($icinga.NodeName)"
$machines = @(Read-FleetManifest -Path $ManifestFile)
if ($machines.Count -eq 0) {
    Write-Error "❌ No machines in '$ManifestFile' - provision the estate first (provision-fleet-machines.ps1)."
    exit 1
}
$catalog = Read-ExplainedCheckCatalog -Path (Join-Path $scriptDir "explained-checks.json")
$faultOnly = $BreakDisk -or $RestoreDisk

# Where the Icinga CA goes on a machine. The tutorial keeps it in the agent's
# security folder; on Linux that folder moves with the packaging, so it sits
# next to nsclient.ini there.
$windowsCa = 'C:\Program Files\NSClient++\security\icinga2-ca.crt'
$linuxCa = '/etc/nsclient/icinga2-ca.crt'

function New-Row {
    param([string]$Path, [string]$Key, [string]$Value)
    [pscustomobject]@{ Path = $Path; Key = $Key; Value = $Value }
}

# The rows of one machine: the tutorial's nsclient.ini, section by section.
function Get-AgentRows {
    param([Parameter(Mandatory)][ValidateSet("windows", "linux")][string]$Os)
    $schedules = "/settings/scheduler/schedules"
    $rows = @()
    foreach ($schedule in $catalog.Schedules) {
        $command = if ($Os -eq "windows") { $schedule.Windows } else { $schedule.Linux }
        if ($schedule.Name -eq $catalog.Fault) {
            if ($BreakDisk) { $command = if ($Os -eq "windows") { $schedule.BrokenWindows } else { $schedule.BrokenLinux } }
            $rows += New-Row $schedules $schedule.Name $command
        }
        elseif (-not $faultOnly) {
            $rows += New-Row $schedules $schedule.Name $command
        }
    }
    if ($faultOnly) { return $rows }

    foreach ($module in "CheckSystem", "CheckDisk", "CheckHelpers", "Scheduler", "IcingaClient") {
        $rows += New-Row "/modules" $module "enabled"
    }
    $rows += New-Row "/settings/Icinga/client" "hostname" '${host_lc}'
    $target = "/settings/Icinga/client/targets/default"
    $rows += New-Row $target "address" $icingaAddress
    $rows += New-Row $target "username" $icinga.ExplainUser
    $rows += New-Row $target "password" $icinga.ExplainPassword
    $rows += New-Row $target "verify mode" $VerifyMode
    $rows += New-Row $target "ca" $(if ($Os -eq "windows") { $windowsCa } else { $linuxCa })
    # The turn-key passive flow sets this through its bundle; the tutorial
    # defines the objects on the Icinga side, and its API user may not create.
    $rows += New-Row $target "ensure objects" "false"

    $default = "/settings/scheduler/schedules/default"
    $rows += New-Row $default "channel" "ICINGA"
    $rows += New-Row $default "interval" $catalog.Interval
    $rows += New-Row $default "report" "all"

    $failed = "/settings/core/failed checks"
    $rows += New-Row $failed "capture metrics" "true"
    $rows += New-Row $failed "prune metrics" "zeros, empty, duplicates"
    $rows += New-Row $failed "capture diagnostics" "true"
    foreach ($key in "processes", "services") { $rows += New-Row "/settings/system/windows/diagnostics" $key "true" }
    foreach ($key in "services", "errors") { $rows += New-Row "/settings/eventlog/diagnostics" $key "true" }
    foreach ($key in "processes", "services", "errors") { $rows += New-Row "/settings/system/unix/diagnostics" $key "true" }
    $rows += New-Row "/settings/disk/diagnostics" "growth spots" "true"
    $rows += New-Row "/settings/fleet" "failure context" "true"
    return $rows
}

function Get-AgentScript {
    param([Parameter(Mandatory)][ValidateSet("windows", "linux")][string]$Os)
    $rows = @(Get-AgentRows -Os $Os)
    if ($Os -eq "windows") {
        $settings = New-EstateLocalSettingsScript -Rows $rows -Shell powershell
        $ca = ""
        if (-not $faultOnly) {
            $ca = @"
`$hosts = Join-Path `$env:SystemRoot 'System32\drivers\etc\hosts'
`$kept = @(Get-Content -Path `$hosts | Where-Object { `$_ -notmatch '\s$([regex]::Escape($icinga.NodeName))\s*`$' })
Set-Content -Path `$hosts -Value (`$kept + '$hostsLine')
New-Item -ItemType Directory -Force -Path (Split-Path -Parent '$windowsCa') | Out-Null
Set-Content -Path '$windowsCa' -Value @'
$caPem
'@
"@
        }
        return @"
`$ErrorActionPreference = 'Continue'
$ca
$settings
Restart-Service nscp -Force
Start-Sleep -Seconds 5
if ((Get-Service nscp).Status -eq 'Running') { Write-Output 'EXPLAINED-AGENT: ok' } else { Write-Output 'EXPLAINED-AGENT: the service did not start' }
"@
    }
    $settings = New-EstateLocalSettingsScript -Rows $rows -Shell bash
    $ca = ""
    if (-not $faultOnly) {
        $ca = @"
sudo sed -i '/[[:space:]]$($icinga.NodeName)[[:space:]]*`$/d' /etc/hosts
echo '$hostsLine' | sudo tee -a /etc/hosts >/dev/null
sudo tee '$linuxCa' >/dev/null <<'EOF_CA'
$caPem
EOF_CA
sudo chmod 644 '$linuxCa'
"@
    }
    return @"
#!/bin/bash
$ca
$settings
sudo systemctl restart nsclient
sleep 5
if systemctl is-active --quiet nsclient; then echo 'EXPLAINED-AGENT: ok'; else echo 'EXPLAINED-AGENT: the service did not start'; fi
"@
}

$what = if ($BreakDisk) { "Breaking '$($catalog.Fault)' on" } elseif ($RestoreDisk) { "Restoring '$($catalog.Fault)' on" } else { "Configuring" }
Write-Host "● Connecting to Azure account..."
if (-not (Get-AzContext)) { Connect-AzAccount | Out-Null }
Write-Host "$what $($machines.Count) machine(s): $(@($machines | ForEach-Object { $_.Name }) -join ', ')..."

# One RunCommand per machine, all at once: each takes a minute or two, nearly
# all of it waiting on Azure.
$pending = @(foreach ($m in $machines) {
        $os = if ("$($m.Os)" -eq "windows") { "windows" } else { "linux" }
        $commandId = if ($os -eq "windows") { "RunPowerShellScript" } else { "RunShellScript" }
        $script = (Get-AgentScript -Os $os) -replace "`r", ""
        [pscustomobject]@{
            Machine = $m
            Os      = $os
            Job     = Invoke-AzVMRunCommand -ResourceGroupName $m.ResourceGroup -VMName $m.Name `
                -CommandId $commandId -ScriptString $script -AsJob -ErrorAction Stop
        }
    })

$failed = @()
foreach ($p in $pending) {
    $name = $p.Machine.Name
    try {
        $result = $p.Job | Wait-Job | Receive-Job -ErrorAction Stop
    }
    catch {
        $failed += "${name}: RunCommand failed: $($_.Exception.Message)"
        continue
    }
    $stdout = Get-EstateVmStdout $result
    $messages = @($result.Value | ForEach-Object { "$($_.Message)" })
    # The password reads back redacted, which is not a failure to write it.
    $missed = @(Get-EstateLocalSettingFailures -Messages $messages | Where-Object { $_ -notmatch '\] password reads back' })
    if ($missed.Count -gt 0) { $failed += "${name}: $($missed -join '; ')" }
    elseif ($stdout -notmatch 'EXPLAINED-AGENT: ok') {
        $tail = (($stdout -split "`n") | Select-Object -Last 6) -join " | "
        $failed += "${name}: the agent did not come back: $tail"
    }
    else { Write-Host "   ${name}: ok" }
}
if ($failed.Count -gt 0) {
    $failed | ForEach-Object { Write-Host "   $_" }
    Write-Error "❌ $($failed.Count) machine(s) were not configured."
    exit 1
}

if ($BreakDisk) {
    Write-Host "✅ '$($catalog.Fault)' now fails on every machine. Put it back with -RestoreDisk."
}
elseif ($RestoreDisk) {
    Write-Host "✅ '$($catalog.Fault)' is back to its normal thresholds."
}
else {
    Write-Host "✅ The agents submit $($catalog.ServiceNames -join ', ') to $icingaAddress every $($catalog.Interval) and report failures to the fleet server."
    Write-Host "Next: break a check and read the notification:"
    Write-Host "  ./build/powershell/icinga/verify-icinga-explained.ps1"
}
