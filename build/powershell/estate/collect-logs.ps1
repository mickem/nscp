<#
.SYNOPSIS
    Pull the logs and fleet state of every machine in an estate manifest into a
    local run folder.
.DESCRIPTION
    For each machine in the manifest this writes .estate-runs/<timestamp>/<machine>/
    with whatever of these exists on the VM:

        nsclient.log         the agent log (last -TailLines lines)
        role-install.log     the role install fragment's captured output
        fleet.ini            what the applied bundles rendered (passwords,
                             secrets and tokens redacted)
        agent-state.json     the enrollment state (private key redacted)
        journal.txt          journalctl -u nsclient, last 500 lines (Linux)
        events.txt           the Application event log, last 500 events (Windows)
        msi-install.log      the MSI log, last -MsiTailLines lines (Windows)
        role-<file>          the extra files the role.json "logs" list names
        fleet-host.json      GET /api/hosts/<id> at collection time

    checks.json sits in the same folder too: the jest estate suite writes it
    and verify-estate.ps1 adds the sync state.

    Everything is gathered into one archive on the VM and fetched through
    RunCommand in 3000-character chunks of about 30 seconds each, so a
    machine costs a few minutes rather than one round trip per file. A machine that cannot be reached gets
    a collect-error.txt instead; collection never fails the run that called it.

    run-estate.ps1 calls this before teardown, on success and failure alike.
    Run it by hand against a kept estate (-KeepVms) at any time.
.PARAMETER ManifestFile
    The estate manifest (default: build/powershell/.fleet-machines.json).
.PARAMETER RunFolder
    Where to write the per-machine folders. Default: a new
    build/powershell/.estate-runs/<timestamp> folder.
.PARAMETER Name
    Only these machines (default: every machine in the manifest).
.PARAMETER FleetServer
    Fleet server url, for fleet-host.json. Defaults to each machine's own
    "FleetServer" in the manifest.
.PARAMETER ApiKey
    Fleet API key for fleet-host.json (a view_only key is enough). Defaults to
    $env:NSCLIENT_FLEET_API_KEY; without one fleet-host.json is skipped.
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate.
.PARAMETER TailLines
    How much of the agent and role logs to keep (default 2000 lines). The
    whole archive travels through RunCommand 3000 characters at a time, about
    30 seconds per round trip, so every kilobyte counts.
.PARAMETER MsiTailLines
    How much of the verbose MSI log to keep (default 200 lines). A whole one
    is about 200 KB, which took most of a Windows machine's 17 minutes of
    collection; the outcome of the install is in its last lines.
.EXAMPLE
    ./estate/collect-logs.ps1
.EXAMPLE
    ./estate/collect-logs.ps1 -ManifestFile .estate-runs/20261001-101500/mysql/manifest.json -Name mysql-fra-02
#>
param(
    [string]$ManifestFile = "",
    [string]$RunFolder = "",
    [string[]]$Name = @(),
    [string]$FleetServer = "",
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [switch]$FleetNoVerify,
    [ValidateRange(100, 100000)]
    [int]$TailLines = 2000,
    [ValidateRange(10, 100000)]
    [int]$MsiTailLines = 200,
    [string]$RolesDir = ""
)

$ErrorActionPreference = "Stop"
$parentDir = Split-Path -Parent $PSScriptRoot
. (Join-Path $parentDir "fleet-api.ps1")
Import-FleetConfig
if (-not $ApiKey) { $ApiKey = $env:NSCLIENT_FLEET_API_KEY }
. (Join-Path $PSScriptRoot "estate-api.ps1")

if (-not $ManifestFile) { $ManifestFile = Join-Path $parentDir ".fleet-machines.json" }
$machines = @(Read-FleetManifest -Path $ManifestFile)
if ($Name.Count -gt 0) { $machines = @($machines | Where-Object { $Name -contains $_.Name }) }
if ($machines.Count -eq 0) {
    Write-Host "Nothing to collect: no machines in $ManifestFile$(if ($Name) { " named $($Name -join ', ')" })."
    return
}
if (-not $RunFolder) { $RunFolder = New-EstateRunFolder }
New-Item -ItemType Directory -Path $RunFolder -Force | Out-Null

# The role catalog only adds the role's extra log files; a manifest written
# before roles existed, or a role since removed, just collects the standard set.
$catalog = @{}
try {
    if (-not $RolesDir) { $RolesDir = Get-EstateRolesDir }
    $catalog = Get-EstateRoleCatalog -RolesDir $RolesDir
}
catch { Write-Warning "Could not read the role catalog ($($_.Exception.Message)); collecting the standard files only." }

& (Join-Path $parentDir "connect-to-azure.ps1")
foreach ($module in @('Az.Compute', 'Az.Resources')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

# Secrets do not belong in a log folder, even a throwaway estate's: -KeepLogs
# keeps it after the machines are gone. fleet.ini carries whatever the role
# bundle set ([/settings/mysql] password, ...), so a key named like a secret
# has its value masked; and the machine's role password is masked wherever it
# turns up (a connection string, an install log, the fleet host's config).
function Protect-CollectedFiles {
    param([string]$Dir, [string]$RolePassword)
    $ini = Join-Path $Dir "fleet.ini"
    if (Test-Path $ini) {
        $text = Get-Content -Path $ini -Raw
        $text = $text -replace '(?im)^([ \t]*[^=;#\r\n]*(?:password|secret|token|private[ _]?key|api[ _]?key)[^=\r\n]*=[ \t]*)\S[^\r\n]*', '$1<redacted>'
        Set-Content -Path $ini -Value $text -NoNewline
    }
    if (-not $RolePassword) { return }
    foreach ($f in @(Get-ChildItem -Path $Dir -File -Recurse)) {
        if ($f.Extension -in @(".tgz", ".zip")) { continue }
        $text = Get-Content -Path $f.FullName -Raw
        if ($text -and $text.Contains($RolePassword)) {
            Set-Content -Path $f.FullName -Value $text.Replace($RolePassword, "<redacted>") -NoNewline
        }
    }
}

function Get-RoleLogFiles {
    param($Machine)
    if (-not $Machine.Role -or -not $catalog.Contains("$($Machine.Role)")) { return @() }
    $family = "$($Machine.Family)"
    if (-not $family) { $family = "$($Machine.Os)" }
    $logs = $catalog["$($Machine.Role)"].Logs
    if (-not $logs.Contains($family)) { return @() }
    return @($logs[$family])
}

function New-LinuxCollectScript {
    param([string[]]$Extra)
    $extraLines = ($Extra | ForEach-Object {
            # The role-install log is already collected under its own name.
            if ($_ -ne "/var/log/nscp-role-install.log") {
                "tail -n $TailLines '$_' > `"`$d/role-`$(basename '$_')`" 2>/dev/null"
            }
        }) -join "`n"
    return @"
#!/bin/bash
d=/tmp/nscp-collect
rm -rf `$d /tmp/nscp-collect.tgz
mkdir -p `$d
tail -n $TailLines /var/log/nsclient/nsclient.log > `$d/nsclient.log 2>/dev/null
cp /var/log/nscp-role-install.log `$d/role-install.log 2>/dev/null
state=`$(find /usr/lib/nsclient /var/lib/nsclient /etc/nsclient -name agent-state.json 2>/dev/null | head -1)
if [ -n "`$state" ]; then
    cp "`$state" `$d/agent-state.json
    cp "`$(dirname "`$(dirname "`$state")")/fleet/fleet.ini" `$d/fleet.ini 2>/dev/null
fi
journalctl -u nsclient --no-pager -n 500 > `$d/journal.txt 2>&1
$extraLines
# Empty files are things that did not exist; leave them out.
find `$d -type f -empty -delete
tar czf /tmp/nscp-collect.tgz -C `$d .
echo COLLECTED
"@
}

function New-WindowsCollectScript {
    param([string[]]$Extra)
    $extraLines = ($Extra | ForEach-Object {
            if ($_ -ne 'C:\nscp-role-install.log') {
                "if (Test-Path '$_') { Get-Content -Path '$_' -Tail $TailLines | Set-Content -Encoding UTF8 -Path (Join-Path `$d ('role-' + (Split-Path -Leaf '$_'))) }"
            }
        }) -join "`n"
    return @"
`$d = 'C:\temp\nscp-collect'
Remove-Item -Recurse -Force `$d, 'C:\temp\nscp-collect.zip' -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path `$d -Force | Out-Null
`$root = 'C:\Program Files\NSClient++'
if (Test-Path "`$root\nsclient.log") { Get-Content -Path "`$root\nsclient.log" -Tail $TailLines | Set-Content -Encoding UTF8 -Path "`$d\nsclient.log" }
if (Test-Path 'C:\temp\install.log') { Get-Content -Path 'C:\temp\install.log' -Tail $MsiTailLines | Set-Content -Encoding UTF8 -Path "`$d\msi-install.log" }
Copy-Item 'C:\nscp-role-install.log' "`$d\role-install.log" -ErrorAction SilentlyContinue
Copy-Item "`$root\security\agent-state.json" "`$d\agent-state.json" -ErrorAction SilentlyContinue
Copy-Item "`$root\fleet\fleet.ini" "`$d\fleet.ini" -ErrorAction SilentlyContinue
# One line per event, message cut short: a table of full messages runs to a
# megabyte, which is hundreds of RunCommand round trips to fetch.
Get-WinEvent -LogName Application -MaxEvents 500 -ErrorAction SilentlyContinue | ForEach-Object {
    `$msg = ("`$(`$_.Message)" -replace '\s+', ' ').Trim()
    if (`$msg.Length -gt 300) { `$msg = `$msg.Substring(0, 300) + '...' }
    '{0:s} {1,-11} {2} {3}: {4}' -f `$_.TimeCreated, `$_.LevelDisplayName, `$_.ProviderName, `$_.Id, `$msg
} | Set-Content -Encoding UTF8 -Path "`$d\events.txt"
$extraLines
Compress-Archive -Path "`$d\*" -DestinationPath 'C:\temp\nscp-collect.zip' -Force
Write-Output COLLECTED
"@
}

$collected = 0
foreach ($m in $machines) {
    # A machine that failed before its VM existed (a refused fleet host, a
    # quota refusal) has no resource group; three RunCommand retries against
    # it would only bury the real failure in ResourceGroupNotFound noise.
    if (-not $m.ResourceGroup -or -not (Get-AzResourceGroup -Name $m.ResourceGroup -ErrorAction SilentlyContinue)) {
        Write-Host "● $($m.Name): no resource group $($m.ResourceGroup) in Azure, nothing to collect"
        continue
    }
    $dir = Join-Path $RunFolder $m.Name
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    Write-Host "● $($m.Name): collecting into $dir"
    $os = "linux"
    if ("$($m.Os)" -eq "windows") { $os = "windows" }
    try {
        $extra = Get-RoleLogFiles -Machine $m
        if ($os -eq "windows") {
            $collectScript = New-WindowsCollectScript -Extra $extra
            $remote = 'C:\temp\nscp-collect.zip'
            $archive = Join-Path $dir "collect.zip"
        }
        else {
            $collectScript = New-LinuxCollectScript -Extra $extra
            $remote = '/tmp/nscp-collect.tgz'
            $archive = Join-Path $dir "collect.tgz"
        }
        $run = Invoke-EstateVmRun -ResourceGroupName $m.ResourceGroup -VmName $m.Name -Os $os -ScriptString $collectScript
        $out = Get-EstateVmStdout $run
        if ($out -notmatch 'COLLECTED') { throw "the collect script did not finish. stdout: $out`nstderr: $(Get-EstateVmStderr $run)" }
        Receive-EstateVmFile -ResourceGroupName $m.ResourceGroup -VmName $m.Name -Os $os `
            -RemotePath $remote -LocalPath $archive
        if ($os -eq "windows") { Expand-Archive -Path $archive -DestinationPath $dir -Force }
        else {
            # bsdtar (Windows 10+) and GNU tar both read a gzip'ed tarball.
            tar -xzf $archive -C $dir
            if ($LASTEXITCODE -ne 0) { throw "tar could not unpack $archive" }
        }
        Remove-Item $archive -Force

        # The private key is not something a log folder should hold, even for a
        # throwaway estate.
        $statePath = Join-Path $dir "agent-state.json"
        if (Test-Path $statePath) {
            $state = Get-Content -Path $statePath -Raw
            $state = $state -replace '("private_key_pem"\s*:\s*)"(?:[^"\\]|\\.)*"', '$1"<redacted>"'
            Set-Content -Path $statePath -Value $state -NoNewline
        }
        $collected++
    }
    catch {
        Write-Warning "   $($m.Name): $($_.Exception.Message)"
        Set-Content -Path (Join-Path $dir "collect-error.txt") -Value "$($_.Exception.Message)"
    }

    $server = $FleetServer
    if (-not $server) { $server = "$($m.FleetServer)" }
    if ($m.HostId -and $server -and $ApiKey) {
        try {
            Get-FleetHost -FleetServer $server -ApiKey $ApiKey -HostId $m.HostId -SkipCertificateCheck:$FleetNoVerify |
            ConvertTo-Json -Depth 16 | Set-Content -Path (Join-Path $dir "fleet-host.json")
        }
        catch { Write-Warning "   $($m.Name): could not read fleet host $($m.HostId): $($_.Exception.Message)" }
    }
    # From the .pwd file: the manifest no longer carries the role password.
    try { Protect-CollectedFiles -Dir $dir -RolePassword "$(Get-EstateMachineRolePassword -Machine $m)" }
    catch { Write-Warning "   $($m.Name): could not redact the collected files: $($_.Exception.Message)" }
    $files = @(Get-ChildItem -Path $dir -File | ForEach-Object { $_.Name })
    Write-Host "   $($files -join ', ')"
}

Write-Host "● Logs for $collected/$($machines.Count) machine(s) in $RunFolder"
