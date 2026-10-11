<#
.SYNOPSIS
    Tear down everything the estate tooling ever left behind: every machine in
    every manifest, every stray resource group with the estate prefix, their
    fleet hosts, and the run folders and credential files that are then unused.
.DESCRIPTION
    What run-estate.ps1 -Cleanup runs. It goes through

        1. every manifest - .fleet-machines.json and every
           .estate-runs/<ts>/suite-<role>/manifest.json - and tears each one
           down with provision-fleet-machines.ps1 -Destroy -RemoveFleetHosts,
           which removes the fleet hosts, deletes the resource groups and
           drops the manifest once everything is confirmed gone;
        2. every resource group in Azure named <prefix>-* that no manifest
           lists (a run that lost its manifest, a -NoWait teardown that never
           got re-run, a suite that died half way): deleted too, along with
           the fleet host whose hostname is that machine's name;
        3. the run folders no manifest is left in, and the .vm.<name>.pwd
           files no remaining machine owns: removed, unless -KeepLogs (which
           keeps the folders; credential files of dead machines go either way).

    It prints the plan first and asks before deleting anything, unless -Force.
    -DryRun prints the plan and stops. Exits 1 if anything could not be
    removed; what remains is still in its manifest, so the next run picks it up.

    The fleet sweep only touches hosts that belong to a machine being removed:
    the fleet server may well hold hosts that are not this tooling's to delete.
.PARAMETER FleetServer
    Fleet server url for the hosts of stray resource groups and as the
    fallback for manifests that do not record one. Defaults to
    $env:NSCLIENT_FLEET_SERVER.
.PARAMETER ApiKey
    Fleet API key (must be allowed to delete hosts). Defaults to
    $env:NSCLIENT_FLEET_API_KEY. Without one, fleet hosts are left alone and
    said so.
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate.
.PARAMETER ResourceGroupPrefix
    The prefix provision-fleet-machines.ps1 gave the resource groups
    (default NSCP-Fleet). Only groups named <prefix>-* are ever considered.
.PARAMETER KeepLogs
    Keep the run folders; only the machines, fleet hosts and credential files
    go.
.PARAMETER DryRun
    Print what would be removed and stop. Touches neither Azure nor the fleet
    server beyond listing.
.PARAMETER Force
    Do not ask before deleting.
.EXAMPLE
    ./estate/cleanup-estate.ps1 -DryRun
.EXAMPLE
    ./estate/cleanup-estate.ps1 -Force -KeepLogs
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [switch]$FleetNoVerify,
    [string]$ResourceGroupPrefix = "NSCP-Fleet",
    [switch]$KeepLogs,
    [switch]$DryRun,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$estateDir = $PSScriptRoot
$parentDir = Split-Path -Parent $estateDir
. (Join-Path $parentDir "fleet-api.ps1")
. (Join-Path $estateDir "estate-api.ps1")
$psExe = (Get-Process -Id $PID).Path
$provision = Join-Path $parentDir "provision-fleet-machines.ps1"
$runsFolder = Join-Path $parentDir ".estate-runs"
$skipCert = [bool]$FleetNoVerify

# --- Inventory: manifests ----------------------------------------------------
$manifestFiles = [System.Collections.Generic.List[string]]::new()
$shared = Join-Path $parentDir ".fleet-machines.json"
if (Test-Path $shared) { $manifestFiles.Add($shared) }
if (Test-Path $runsFolder) {
    Get-ChildItem -Path $runsFolder -Directory | ForEach-Object {
        Get-ChildItem -Path $_.FullName -Filter manifest.json -Recurse -File | ForEach-Object { $manifestFiles.Add($_.FullName) }
    }
}
$manifests = [System.Collections.Generic.List[object]]::new()
foreach ($file in $manifestFiles) {
    $machines = @(Read-FleetManifest -Path $file)
    if ($machines.Count -eq 0) {
        # Empty or unreadable: nothing is behind it, so it is just a file to drop.
        $manifests.Add([pscustomobject]@{ File = $file; Machines = @(); Empty = $true })
        continue
    }
    $manifests.Add([pscustomobject]@{ File = $file; Machines = $machines; Empty = $false })
}
$listedGroups = @($manifests | ForEach-Object { $_.Machines } | ForEach-Object { "$($_.ResourceGroup)" } | Where-Object { $_ })

# --- Inventory: Azure ----------------------------------------------------------
& (Join-Path $parentDir "connect-to-azure.ps1")
foreach ($module in @('Az.Resources')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}
$allGroups = @(Get-AzResourceGroup | Where-Object { $_.ResourceGroupName -like "$ResourceGroupPrefix-*" })
$strayGroups = @($allGroups | Where-Object { $listedGroups -notcontains $_.ResourceGroupName })
# A machine's name is what follows the prefix; that is also its hostname.
$strayNames = @($strayGroups | ForEach-Object { $_.ResourceGroupName.Substring($ResourceGroupPrefix.Length + 1) })

# --- Inventory: fleet hosts of the stray groups -------------------------------
$strayHosts = @()
$fleetNote = $null
if ($strayNames.Count -gt 0) {
    if (-not $FleetServer -or -not $ApiKey) {
        $fleetNote = "no fleet server url or API key: the fleet hosts of the stray machines are left alone"
    }
    else {
        try {
            $strayHosts = @(Get-FleetHosts -FleetServer $FleetServer -ApiKey $ApiKey -SkipCertificateCheck:$skipCert |
                    Where-Object { $_.hostname -and ($strayNames -contains "$($_.hostname)") })
        }
        catch {
            $fleetNote = "could not list the fleet hosts ($($_.Exception.Message)); the fleet hosts of the stray machines are left alone"
        }
    }
}

# --- Plan ----------------------------------------------------------------------
$machineCount = ($manifests | ForEach-Object { $_.Machines.Count } | Measure-Object -Sum).Sum
Write-Host "● Cleanup plan"
if ($manifests.Count -gt 0) {
    Write-Host "   $($manifests.Count) manifest(s), $machineCount machine(s):"
    foreach ($mf in $manifests) {
        $rel = $mf.File.Replace("$parentDir$([IO.Path]::DirectorySeparatorChar)", "")
        if ($mf.Empty) { Write-Host "   - $rel  (empty; will be removed)"; continue }
        Write-Host "   - $rel"
        foreach ($m in $mf.Machines) {
            Write-Host "       $($m.Name)  (RG $($m.ResourceGroup), fleet host $(if ($m.HostId) { $m.HostId } else { 'none' }))"
        }
    }
}
else { Write-Host "   no manifests" }
if ($strayGroups.Count -gt 0) {
    Write-Host "   $($strayGroups.Count) resource group(s) named $ResourceGroupPrefix-* that no manifest lists:"
    foreach ($g in $strayGroups) {
        $name = $g.ResourceGroupName.Substring($ResourceGroupPrefix.Length + 1)
        $h = @($strayHosts | Where-Object { "$($_.hostname)" -eq $name })
        $hostNote = if ($h.Count -gt 0) { "fleet host $(($h | ForEach-Object { $_.id }) -join ', ')" } else { "no fleet host found by that hostname" }
        Write-Host "   - $($g.ResourceGroupName)  ($($g.Location); $hostNote)"
    }
}
else { Write-Host "   no stray $ResourceGroupPrefix-* resource groups in Azure" }
if ($fleetNote) { Write-Warning $fleetNote }

$runDirs = @()
if (Test-Path $runsFolder) { $runDirs = @(Get-ChildItem -Path $runsFolder -Directory) }
$pwdFiles = @(Get-ChildItem -Path $parentDir -Filter ".vm.*.pwd" -File -Force)
if (-not $KeepLogs) { Write-Host "   run folders with nothing live left in them will be removed ($($runDirs.Count) run folder(s) in total)" }
else { Write-Host "   -KeepLogs: run folders stay" }
Write-Host "   .vm.<name>.pwd files of machines that no longer exist will be removed ($($pwdFiles.Count) file(s) in total)"

if ($DryRun) {
    Write-Host "● -DryRun: nothing was removed."
    return
}
if ($manifests.Count -eq 0 -and $strayGroups.Count -eq 0 -and $runDirs.Count -eq 0 -and $pwdFiles.Count -eq 0) {
    Write-Host "✅ Nothing to clean up."
    return
}
if (-not $Force) {
    $answer = Read-Host "Remove all of this? [y/N]"
    if ($answer -notmatch '^(y|yes)$') { Write-Host "Aborted."; return }
}

$failures = [System.Collections.Generic.List[string]]::new()

# --- 1. Manifests: the ordinary teardown, one manifest at a time ----------------
# A child process, as everywhere else: the script calls `exit`. Each one
# deletes its resource groups concurrently, so the serial loop costs about
# one Azure delete per manifest, not per machine.
foreach ($mf in $manifests) {
    if ($mf.Empty) {
        Remove-Item $mf.File -Force -ErrorAction SilentlyContinue
        continue
    }
    Write-Host "● Tearing down $($mf.File)"
    $destroyArgs = @("-Destroy", "-RemoveFleetHosts", "-Force", "-ManifestFile", $mf.File, "-ResourceGroupPrefix", $ResourceGroupPrefix)
    if ($FleetServer) { $destroyArgs += @("-FleetServer", $FleetServer) }
    if ($ApiKey) { $destroyArgs += @("-ApiKey", $ApiKey) }
    if ($skipCert) { $destroyArgs += "-FleetNoVerify" }
    $ErrorActionPreference = "Continue"
    & $psExe -NoProfile -File $provision @destroyArgs 2>&1 | ForEach-Object { Write-Host "   $_" }
    $rc = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($rc -ne 0) { $failures.Add("$($mf.File): teardown exited $rc; what is left is still listed there") }
}

# --- 2. Stray resource groups and their fleet hosts -------------------------------
if ($strayGroups.Count -gt 0) {
    foreach ($h in $strayHosts) {
        try {
            Remove-FleetHost -FleetServer $FleetServer -ApiKey $ApiKey -HostId $h.id -SkipCertificateCheck:$skipCert
            Write-Host "   ✅ removed fleet host $($h.id) ($($h.hostname))"
        }
        catch {
            Write-Warning "   could not remove fleet host $($h.id) ($($h.hostname)): $($_.Exception.Message)"
            $failures.Add("fleet host $($h.id) ($($h.hostname)) could not be removed")
        }
    }
    $pending = [System.Collections.Generic.List[object]]::new()
    foreach ($g in $strayGroups) {
        try {
            Write-Host "   deleting resource group $($g.ResourceGroupName)..."
            $job = Remove-AzResourceGroup -Name $g.ResourceGroupName -Force -AsJob -ErrorAction Stop
            $pending.Add([pscustomobject]@{ Name = $g.ResourceGroupName; Job = $job })
        }
        catch {
            Write-Warning "   failed to start deleting $($g.ResourceGroupName): $($_.Exception.Message)"
            $failures.Add("resource group $($g.ResourceGroupName) could not be deleted")
        }
    }
    if ($pending.Count -gt 0) {
        Write-Host "● Waiting for $($pending.Count) resource group deletion(s) to finish (they run concurrently)..."
        $spent = 0
        while ($pending | Where-Object { $_.Job.State -eq "Running" }) {
            Start-Sleep -Seconds 10
            $spent += 10
            $left = @($pending | Where-Object { $_.Job.State -eq "Running" })
            Write-Host ("   {0,3}s elapsed, {1} still deleting: {2}" -f $spent, $left.Count, (($left | ForEach-Object { $_.Name }) -join ", "))
        }
        foreach ($p in $pending) {
            try {
                Receive-Job -Job $p.Job -ErrorAction Stop | Out-Null
                if ($p.Job.State -ne "Completed") { throw "deletion job ended in state '$($p.Job.State)'" }
                Write-Host "   ✅ $($p.Name) deleted"
            }
            catch {
                Write-Warning "   failed to delete $($p.Name): $($_.Exception.Message)"
                $failures.Add("resource group $($p.Name) could not be deleted")
            }
            finally { Remove-Job -Job $p.Job -Force -ErrorAction SilentlyContinue }
        }
    }
}

# --- 3. Folders and credential files nothing refers to any more -------------------
# Whatever a teardown could not remove is still in its manifest, so "no
# manifest left" is exactly "nothing live left".
$remainingManifests = @($manifestFiles | Where-Object { Test-Path $_ })
$liveNames = @($remainingManifests | ForEach-Object { Read-FleetManifest -Path $_ } | ForEach-Object { "$($_.Name)" })
if (-not $KeepLogs) {
    foreach ($dir in $runDirs) {
        if (@(Get-ChildItem -Path $dir.FullName -Filter manifest.json -Recurse -File).Count -gt 0) {
            Write-Host "   keeping $($dir.Name): a manifest with live machines is still in it"
            continue
        }
        Remove-Item -Path $dir.FullName -Recurse -Force
        Write-Host "   removed run folder $($dir.Name)"
    }
    if ((Test-Path $runsFolder) -and @(Get-ChildItem -Path $runsFolder -Force).Count -eq 0) {
        Remove-Item -Path $runsFolder -Force
    }
}
# Listed again: a teardown removes the files of the machines it deleted.
foreach ($f in @(Get-ChildItem -Path $parentDir -Filter ".vm.*.pwd" -File -Force)) {
    # .vm.<name>.pwd
    $name = $f.Name.Substring(4, $f.Name.Length - 8)
    if ($liveNames -contains $name) { continue }
    Remove-Item -Path $f.FullName -Force -ErrorAction SilentlyContinue
    Write-Host "   removed $($f.Name)"
}
# The shared file points at whichever machine a sequential run made last;
# with no machine left it points at nothing.
$sharedPwd = Join-Path $parentDir ".vm.pwd"
if ($liveNames.Count -eq 0 -and (Test-Path $sharedPwd)) {
    Remove-Item -Path $sharedPwd -Force
    Write-Host "   removed .vm.pwd"
}

if ($failures.Count -gt 0) {
    Write-Host "❌ $($failures.Count) item(s) could not be removed:" -ForegroundColor Red
    foreach ($f in $failures) { Write-Host "   - $f" -ForegroundColor Red }
    exit 1
}
Write-Host "✅ Estate cleaned up."
exit 0
