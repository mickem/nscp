<#
.SYNOPSIS
    Run one estate suite end to end: provision a role's machines, wait for them
    to enroll and sync, verify them, collect their logs and tear them down.
.DESCRIPTION
    The per-suite worker behind run-estate.ps1, which writes the suite file and
    runs one of these per role, in sequence or concurrently. It is a separate
    script (and process) so a suite's `exit`s and errors stay its own.

    The suite file (suite.json, next to which everything else of the suite is
    written) carries:

        role, spec, count   the suite's role, what to provision (the role
                            and its companions) and how many machines that is
        runFolder           where collect-logs.ps1 puts the machine folders
        provisionArgs       extra provision-fleet-machines.ps1 arguments
        fleetServer, fleetNoVerify, waitMinutes, keepVms, sharedPwdFile,
        checkNsclient       run the role checks through check_nsclient too

    The API key is not in the file; it is inherited as NSCLIENT_FLEET_API_KEY.

    Stages, in order; the verdict for a machine names the first one it failed:

        provision  the VM could not be created or NSClient++ not installed
        install    the role's install fragment failed
        enroll     the agent did not enroll with the fleet server
        sync       the agent enrolled but never reported in sync
        bundle     the role bundle could not be composed (add-role-bundles.ps1)
        check      a role check did not return what it should (verify-estate.ps1)

    It also writes timing.json: when it started, how many seconds it took,
    and the seconds of each stage (provision, sync, verify, collect, teardown).

    The suite writes result.json (one entry per machine) and exits 1 if any
    machine failed.
.PARAMETER SuiteFile
    The suite.json run-estate.ps1 wrote.
#>
param(
    [Parameter(Mandatory)][string]$SuiteFile
)

$ErrorActionPreference = "Stop"
$estateDir = $PSScriptRoot
$parentDir = Split-Path -Parent $estateDir
. (Join-Path $parentDir "fleet-api.ps1")
. (Join-Path $estateDir "estate-api.ps1")

$suite = Get-Content -Path $SuiteFile -Raw | ConvertFrom-Json
$suiteDir = Split-Path -Parent (Resolve-Path $SuiteFile).Path
$manifestFile = Join-Path $suiteDir "manifest.json"
$resultFile = Join-Path $suiteDir "result.json"
$apiKey = $env:NSCLIENT_FLEET_API_KEY
$skipCert = [bool]$suite.fleetNoVerify
$psExe = (Get-Process -Id $PID).Path
$provision = Join-Path $parentDir "provision-fleet-machines.ps1"

function Show { param($line) Write-Host "[$($suite.role)] $line" }

# Child scripts call `exit`, so they run as processes; stderr is merged so a
# warning never becomes an error record that stops this script.
#
# Everything a child prints also goes to suite.log, so the reason for a failed
# stage survives the console scrolling away; $script:ChildError keeps the
# first error the child wrote (else its last line) for the verdict.
$suiteLog = Join-Path $suiteDir "suite.log"
$script:ChildError = $null
function Invoke-Child {
    param([string]$Script, [string[]]$Arguments)
    $ErrorActionPreference = "Continue"
    $script:ChildError = $null
    $last = $null
    # stderr arrives one record per line: gather the first error's lines (its
    # message, then the server's response body) up to PowerShell's
    # "At <file>:<line>" position trailer.
    $errorLines = [System.Collections.Generic.List[string]]::new()
    $errorState = "before"
    Add-Content -Path $suiteLog -Value "===== $(Get-Date -Format s) $(Split-Path -Leaf $Script) $($Arguments -join ' ')"
    & $psExe -NoProfile -File $Script @Arguments 2>&1 | ForEach-Object {
        $line = "$_"
        Show $line
        Add-Content -Path $suiteLog -Value $line
        if (-not $line.Trim()) { return }
        $last = $line.Trim()
        if ($errorState -ne "done" -and $_ -is [System.Management.Automation.ErrorRecord]) {
            if ($last -match '^At .+:') { if ($errorState -eq "in") { $errorState = "done" } }
            else { $errorState = "in"; $errorLines.Add($last) }
        }
        elseif ($errorState -eq "in") { $errorState = "done" }
    }
    $firstError = $errorLines -join " "
    $rc = $LASTEXITCODE
    if ($rc -ne 0) { $script:ChildError = if ($firstError) { $firstError } else { $last } }
    return $rc
}

# "<script> failed: <first error>" for a verdict, cut to one readable line.
function Get-ChildFailure {
    param([string]$Script)
    $why = "$script:ChildError" -replace '\s+', ' '
    if ($why.Length -gt 300) { $why = $why.Substring(0, 300) + "..." }
    if ($why) { return "$Script failed: $why (full output: suite-$($suite.role)/suite.log)" }
    return "$Script failed (full output: suite-$($suite.role)/suite.log)"
}

$results = [ordered]@{}
function Set-Verdict {
    param($Machine, [string]$Stage, [string]$Detail)
    $entry = $results[$Machine.Name]
    if ($entry.Verdict -eq "FAILED") { return }  # the first failing stage wins
    if ($Stage) {
        $entry.Verdict = "FAILED"
        $entry.Stage = $Stage
        $entry.Detail = $Detail
    }
}

# Wall-clock per stage, for timing.json and the run's summary: most of a
# suite is waiting on Azure, and which stage it waits in is worth knowing.
$suiteStart = Get-Date
$stageTimes = [ordered]@{}
$script:StageName = $null
$script:StageStart = $null
function Stop-Stage {
    if ($script:StageName) {
        $stageTimes[$script:StageName] = [int][Math]::Round(((Get-Date) - $script:StageStart).TotalSeconds)
        $script:StageName = $null
    }
}
function Start-Stage {
    param([string]$Name)
    Stop-Stage
    $script:StageName = $Name
    $script:StageStart = Get-Date
}

# --- provision, sync, verify --------------------------------------------------
# Everything up to collect is in one try: under ErrorActionPreference=Stop an
# unexpected error (a truncated verify.json, a fleet API hiccup) would
# otherwise end the script here and leave the machines up and billing, with
# no logs and no result.json. Caught, it fails the machines still standing at
# the stage it happened in, and collect and teardown run as usual.
$machines = @()
try {
    # --- provision -----------------------------------------------------------
    Start-Stage "provision"
    Show "provisioning $($suite.count) machine(s)..."
    # spec, not role=count: count includes the companions, which the spec brings.
    $spec = "$($suite.spec)"
    if (-not $spec) { $spec = "$($suite.role)=$($suite.count)" }
    $provisionArgs = @("-Roles", $spec, "-ManifestFile", $manifestFile,
        "-MaxMachines", "$($suite.count)") + @($suite.provisionArgs)
    $rc = Invoke-Child -Script $provision -Arguments $provisionArgs
    $machines = @(Read-FleetManifest -Path $manifestFile)
    foreach ($m in $machines) {
        $results[$m.Name] = [pscustomobject]@{
            Machine = $m.Name; Role = $m.Role; Image = $m.Image; Verdict = "PASSED"; Stage = $null; Detail = $null
        }
        if ($m.Status -ne "OK") {
            $stage = "$($m.FailedStage)"
            if (-not $stage) { $stage = "provision" }
            $why = "$($m.Detail)" -replace '\s+', ' '
            if (-not $why) { $why = "provision-fleet-machines.ps1: $($m.Status)" }
            Set-Verdict -Machine $m -Stage $stage -Detail $why
        }
    }
    if ($machines.Count -eq 0) {
        # Refused before anything was created (bad arguments, quota, budget).
        $results["(none)"] = [pscustomobject]@{
            Machine = "(none)"; Role = $suite.role; Image = $null; Verdict = "FAILED"; Stage = "provision"
            Detail = "provision-fleet-machines.ps1 exited $rc before creating a machine"
        }
    }
    $live = @($machines | Where-Object { $results[$_.Name].Verdict -eq "PASSED" })

    # A sequential run keeps the shared .vm.pwd pointing at the latest machine, so
    # connect-machine.ps1 and `npm run test:live` (tests/) find it.
    if ($suite.sharedPwdFile) {
        foreach ($m in $live) {
            if (Test-Path $m.PwdFile) { Copy-Item $m.PwdFile $suite.sharedPwdFile -Force }
        }
    }

    # --- enroll + sync -------------------------------------------------------
    Start-Stage "sync"
    foreach ($m in @($live | Where-Object { -not $_.HostId })) {
        Set-Verdict -Machine $m -Stage "enroll" -Detail "no fleet host id in the manifest"
    }
    $live = @($live | Where-Object { $_.HostId })
    if ($live.Count -gt 0) {
        Show "waiting for $($live.Count) machine(s) to enroll and sync (up to $($suite.waitMinutes) min)..."
        $byHost = @{}
        foreach ($m in $live) { if ($m.HostId) { $byHost["$($m.HostId)"] = $m } }
        $states = @(Wait-EstateFleetHosts -FleetServer $suite.fleetServer -ApiKey $apiKey -HostIds @($byHost.Keys) `
                -Until in_sync -WaitMinutes $suite.waitMinutes -SkipCertificateCheck:$skipCert -Log { param($l) Show $l })
        foreach ($s in $states) {
            $m = $byHost[$s.HostId]
            if (-not $s.EnrolledAt) { Set-Verdict -Machine $m -Stage "enroll" -Detail "never enrolled (status '$($s.Status)')" }
            elseif (-not $s.Reached) { Set-Verdict -Machine $m -Stage "sync" -Detail "enrolled, but status '$($s.Status)' instead of in_sync" }
            else { Show "$($m.Name): enrolled and in sync" }
        }
        $live = @($live | Where-Object { $results[$_.Name].Verdict -eq "PASSED" })
    }

    # --- bundle + verify (when the role has them and the scripts exist) ------
    Start-Stage "verify"
    $addBundles = Join-Path $estateDir "add-role-bundles.ps1"
    $verify = Join-Path $estateDir "verify-estate.ps1"
    $common = @("-ManifestFile", $manifestFile, "-FleetServer", $suite.fleetServer)
    if ($skipCert) { $common += "-FleetNoVerify" }
    if ($live.Count -gt 0 -and (Test-Path $addBundles)) {
        if ((Invoke-Child -Script $addBundles -Arguments $common) -ne 0) {
            $why = Get-ChildFailure -Script "add-role-bundles.ps1"
            foreach ($m in $live) { Set-Verdict -Machine $m -Stage "bundle" -Detail $why }
            $live = @()
        }
    }
    if ($live.Count -gt 0 -and (Test-Path $verify)) {
        $verifyResult = Join-Path $suiteDir "verify.json"
        $verifyArgs = $common + @("-WaitMinutes", "$($suite.waitMinutes)", "-RunFolder", $suite.runFolder,
            "-ResultFile", $verifyResult, "-Name", (($live | ForEach-Object { $_.Name }) -join ","))
        if ($suite.checkNsclient) { $verifyArgs += "-CheckNsclient" }
        $rc = Invoke-Child -Script $verify -Arguments $verifyArgs
        $verdicts = @{}
        if (Test-Path $verifyResult) {
            foreach ($v in @(Get-Content -Path $verifyResult -Raw | ConvertFrom-Json | ForEach-Object { $_ })) { $verdicts[$v.Machine] = $v }
        }
        foreach ($m in $live) {
            $v = $verdicts[$m.Name]
            if ($v) {
                if (-not $v.Passed) { Set-Verdict -Machine $m -Stage $v.Stage -Detail "$($v.Detail) (see $($m.Name)/checks.json)" }
            }
            elseif ($rc -ne 0) {
                # The gate died before writing its verdicts.
                Set-Verdict -Machine $m -Stage "check" -Detail (Get-ChildFailure -Script "verify-estate.ps1")
            }
        }
    }
}
catch {
    $stage = switch ($script:StageName) { "verify" { "check" } $null { "provision" } default { $script:StageName } }
    $why = "run-suite.ps1 stopped in $($script:StageName): $($_.Exception.Message)" -replace '\s+', ' '
    Show "ERROR: $why"
    Add-Content -Path $suiteLog -Value "ERROR: $why`n$($_.ScriptStackTrace)"
    foreach ($m in $machines) {
        if (-not $results.Contains($m.Name)) {
            $results[$m.Name] = [pscustomobject]@{
                Machine = $m.Name; Role = $m.Role; Image = $m.Image; Verdict = "PASSED"; Stage = $null; Detail = $null
            }
        }
        Set-Verdict -Machine $m -Stage $stage -Detail $why
    }
    if ($results.Count -eq 0) {
        $results["(none)"] = [pscustomobject]@{
            Machine = "(none)"; Role = $suite.role; Image = $null; Verdict = "FAILED"; Stage = $stage; Detail = $why
        }
    }
}

# --- collect, always, before teardown ----------------------------------------
# A manifest that could not be read still names what was created, so collect
# and teardown run whenever it exists, not only when $machines got filled.
$haveMachines = $machines.Count -gt 0 -or (Test-Path $manifestFile)
Start-Stage "collect"
if ($haveMachines) {
    try {
        $collectArgs = @("-ManifestFile", $manifestFile, "-RunFolder", $suite.runFolder)
        if ($skipCert) { $collectArgs += "-FleetNoVerify" }
        if ((Invoke-Child -Script (Join-Path $estateDir "collect-logs.ps1") -Arguments $collectArgs) -ne 0) {
            Show "WARN: $(Get-ChildFailure -Script "collect-logs.ps1")"
        }
    }
    catch { Show "WARN: collecting logs failed: $($_.Exception.Message)" }
}

# --- teardown -------------------------------------------------------------------
Start-Stage "teardown"
if ($haveMachines) {
    if ($suite.keepVms) {
        Show "keeping the machines; tear them down with:"
        Show "  ./provision-fleet-machines.ps1 -Destroy -RemoveFleetHosts -ManifestFile '$manifestFile'"
    }
    else {
        Show "tearing down..."
        $destroyArgs = @("-Destroy", "-RemoveFleetHosts", "-Force", "-ManifestFile", $manifestFile)
        if ($skipCert) { $destroyArgs += "-FleetNoVerify" }
        $torn = $false
        try { $torn = (Invoke-Child -Script $provision -Arguments $destroyArgs) -eq 0 }
        catch { Show "WARN: $($_.Exception.Message)" }
        if (-not $torn) {
            Show "WARN: teardown did not finish; re-run: ./provision-fleet-machines.ps1 -Destroy -RemoveFleetHosts -ManifestFile '$manifestFile'"
        }
    }
}

Stop-Stage
$suiteSeconds = [int][Math]::Round(((Get-Date) - $suiteStart).TotalSeconds)
[ordered]@{ started = $suiteStart.ToString("s"); seconds = $suiteSeconds; stages = $stageTimes } |
    ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $suiteDir "timing.json")
Show ("finished in {0} ({1})" -f (Format-EstateDuration $suiteSeconds),
    (($stageTimes.Keys | ForEach-Object { "$_ $(Format-EstateDuration $stageTimes[$_])" }) -join ", "))

$all = @($results.Values)
ConvertTo-Json -InputObject $all -Depth 4 | Set-Content -Path $resultFile
foreach ($r in $all) {
    if ($r.Verdict -eq "PASSED") { Show "$($r.Machine): PASSED" }
    else { Show "$($r.Machine): FAILED at $($r.Stage) - $($r.Detail)" }
}
if (@($all | Where-Object { $_.Verdict -ne "PASSED" }).Count -gt 0) { exit 1 }
exit 0
