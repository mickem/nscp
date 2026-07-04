<#
.SYNOPSIS
    Provision -> install -> live-test -> tear down NSClient++ across one or
    several Azure VM types, in one command.
.DESCRIPTION
    A thin orchestrator over the existing building blocks in this directory:

        setup-*-machine.ps1   provision a VM, install the NSClient++ release,
                              enable the WEB API + check modules, write creds
        run-tests.ps1         point the live acceptance suite (tests/live) at it
        teardown-machine.ps1  delete the VM's resource group

    By default machines run **sequentially** and share build/powershell/.vm.pwd
    (so connect-machine.ps1 / a plain run-tests still find the last one). With
    -Parallel they run concurrently, each with its own .vm.<name>.pwd credentials
    file so the setups don't clobber each other — this cuts the wall-clock a lot,
    since most of the time is Azure creating and destroying VMs. Each machine
    gets its own resource group, so a failure never collides with another run.

    Azure login: with -Parallel the script logs in once up front (device-code on
    WSL/Linux) so N concurrent setups don't each prompt; sequentially the first
    machine triggers it. Run ./install-azure.ps1 once beforehand if the Az
    modules are not installed.
.PARAMETER Target
    Which machine family/families to run:
      all      - every machine below (default)
      windows  - Windows, latest + oldest (see -WindowsVersions)
      ubuntu   - Ubuntu               (see -UbuntuVersion)
      rocky    - Rocky Linux          (see -RockyVersion)
      redhat   - alias for rocky
.PARAMETER Version
    NSClient++ release to install on every VM (e.g. 0.14.0). Must match a
    published release asset for each OS.
.PARAMETER Location
    Azure region for every VM. Defaults to "WestEurope".
.PARAMETER WindowsVersions
    Windows images to test when the target includes windows. Defaults to the
    latest and oldest supported Windows Server: "windows-2025", "windows-2019".
    Other accepted values: windows-2022, windows-11, windows-10 (see README for
    the client-licensing caveat).
.PARAMETER UbuntuVersion
    Ubuntu image when the target includes ubuntu. Defaults to "24.04".
.PARAMETER RockyVersion
    Rocky image when the target includes rocky/redhat. Defaults to "9".
.PARAMETER Parallel
    Provision/test/tear down the machines concurrently (PowerShell 7+ only).
.PARAMETER MaxParallel
    Max machines in flight at once with -Parallel. Defaults to 4. Mind your
    Azure vCPU quota (each VM is a 2-vCPU size).
.PARAMETER KeepVms
    Do not tear down VMs after testing (for debugging). Sequentially only the
    last machine's creds survive in .vm.pwd; with -Parallel each machine's creds
    stay in its own .vm.<name>.pwd (pass that to connect-machine.ps1 -PwdFile).
.PARAMETER StopOnFirstFailure
    Abort as soon as one machine fails. Sequential only (ignored with -Parallel,
    where machines are already all in flight). Default: run every machine and
    report a pass/fail summary; exit code is non-zero if any failed.
.EXAMPLE
    # Everything, sequentially:
    ./run-all-tests.ps1 -Target all -Version 0.14.0
.EXAMPLE
    # Everything, in parallel (much faster):
    ./run-all-tests.ps1 -Target all -Version 0.14.0 -Parallel
.EXAMPLE
    # Just Windows, only the latest server, keep the VM for inspection:
    ./run-all-tests.ps1 -Target windows -Version 0.14.0 -WindowsVersions windows-2025 -KeepVms
#>
param(
    [ValidateSet("all", "windows", "ubuntu", "rocky", "redhat")]
    [string]$Target = "all",
    [string]$Version = "0.14.0",
    [string]$Location = "WestEurope",
    [string[]]$WindowsVersions = @("windows-2025", "windows-2019"),
    [string]$UbuntuVersion = "24.04",
    [string]$RockyVersion = "9",
    [switch]$Parallel,
    [ValidateRange(1, 16)]
    [int]$MaxParallel = 4,
    [switch]$KeepVms,
    [switch]$StopOnFirstFailure
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot

if ($Parallel -and $PSVersionTable.PSVersion.Major -lt 7) {
    throw "-Parallel needs PowerShell 7+ (ForEach-Object -Parallel); you are on $($PSVersionTable.PSVersion). Re-run with pwsh, or drop -Parallel."
}

# Re-invoke the sub-scripts in a *separate* PowerShell process rather than with
# the call operator: they use `exit`, which in an in-process `&` call would tear
# down this orchestrator too. A child process isolates that and lets us read its
# exit code via $LASTEXITCODE. Use the exact host we are running under.
$psExe = (Get-Process -Id $PID).Path
$runTests = Join-Path $scriptDir "run-tests.ps1"
$teardown = Join-Path $scriptDir "teardown-machine.ps1"
$keep = [bool]$KeepVms

# --- Build the machine matrix from -Target -----------------------------------
$family = if ($Target -eq "redhat") { "rocky" } else { $Target }
$machines = [System.Collections.Generic.List[object]]::new()

function Add-Machine {
    param($Label, $Script, $Os, $VmName, $Rg, [string[]]$SetupArgs)
    # Parallel runs need a per-machine creds file so concurrent setups don't
    # clobber the shared .vm.pwd; sequential keeps the shared file so
    # connect-machine.ps1 and a bare run-tests still find it.
    $credsFile = if ($Parallel) { Join-Path $scriptDir ".vm.$VmName.pwd" } else { Join-Path $scriptDir ".vm.pwd" }
    $machines.Add([pscustomobject]@{
            Label             = $Label
            Script            = $Script
            Os                = $Os
            VmName            = $VmName
            ResourceGroupName = $Rg
            PwdFile           = $credsFile
            SetupArgs         = @($SetupArgs) + @("-PwdFile", $credsFile)
        })
}

if ($family -in @("all", "windows")) {
    foreach ($wv in $WindowsVersions) {
        # Windows computer names are capped at 15 chars, so keep VmName short.
        $short = "Win" + ($wv -replace '^windows-', '')
        $vm = "NSCP-$short"
        $rg = "NSCP-$short-RG"
        Add-Machine -Label "Windows $wv" -Os "windows" -VmName $vm -Rg $rg `
            -Script (Join-Path $scriptDir "win/setup-machine.ps1") `
            -SetupArgs @("-VmName", $vm, "-ResourceGroupName", $rg, "-Version", $Version,
            "-Location", $Location, "-WindowsVersion", $wv)
    }
}
if ($family -in @("all", "ubuntu")) {
    $vm = "NSCP-Ubuntu-Test"; $rg = "NSCP-Ubuntu-RG"
    Add-Machine -Label "Ubuntu $UbuntuVersion" -Os "linux" -VmName $vm -Rg $rg `
        -Script (Join-Path $scriptDir "linux/setup-ubuntu-machine.ps1") `
        -SetupArgs @("-VmName", $vm, "-ResourceGroupName", $rg, "-Version", $Version,
        "-Location", $Location, "-UbuntuVersion", $UbuntuVersion)
}
if ($family -in @("all", "rocky")) {
    $vm = "NSCP-Rocky-Test"; $rg = "NSCP-Rocky-RG"
    Add-Machine -Label "Rocky $RockyVersion" -Os "linux" -VmName $vm -Rg $rg `
        -Script (Join-Path $scriptDir "linux/setup-rocky-machine.ps1") `
        -SetupArgs @("-VmName", $vm, "-ResourceGroupName", $rg, "-Version", $Version,
        "-Location", $Location, "-RockyVersion", $RockyVersion)
}

if ($machines.Count -eq 0) {
    throw "No machines selected for target '$Target'."
}

$mode = if ($Parallel) { "parallel (max $MaxParallel at once)" } else { "sequential" }
Write-Host "● Plan: $($machines.Count) machine(s) [$Target], NSClient++ $Version, region $Location, $mode"
$machines | ForEach-Object { Write-Host "   - $($_.Label)  (VM $($_.VmName), RG $($_.ResourceGroupName))" }

# --- Per-machine worker: provision -> test -> teardown, returns a result -----
# Runs one child PowerShell per step. Every child call MERGES stderr into stdout
# (2>&1) and prints via Write-Host: a sub-script's warnings/errors reach the
# console as plain text and never become error records — which, under a parallel
# runspace, would otherwise bubble up and abort the whole run. The function
# handles all failures itself and always returns a result object; it never
# throws to its caller.
function Invoke-Machine {
    param($M, $PsExe, $RunTests, $Teardown, [bool]$Keep)
    # Be permissive: rely on explicit $LASTEXITCODE checks, not throw-on-stderr.
    $ErrorActionPreference = 'Continue'
    $status = "PROVISION_FAILED"
    $detail = $null

    function Show { param($line) Write-Host "[$($M.Label)] $line" }

    try {
        Show "provisioning + installing..."
        & $PsExe -NoProfile -File $M.Script @($M.SetupArgs) 2>&1 | ForEach-Object { Show $_ }
        if ($LASTEXITCODE -ne 0) { throw "setup exited $LASTEXITCODE" }

        Show "running live acceptance suite..."
        $status = "TEST_FAILED"
        & $PsExe -NoProfile -File $RunTests -VmName $M.VmName -ResourceGroupName $M.ResourceGroupName -Os $M.Os -PwdFile $M.PwdFile 2>&1 | ForEach-Object { Show $_ }
        if ($LASTEXITCODE -ne 0) { throw "tests exited $LASTEXITCODE" }

        $status = "PASSED"
        Show "PASSED"
    }
    catch {
        $detail = "$($_.Exception.Message)"
        Show "FAILED ($status): $detail"
    }
    finally {
        if ($Keep) {
            Show "keeping VM (RG $($M.ResourceGroupName)); creds in $($M.PwdFile)"
        }
        else {
            Show "tearing down $($M.ResourceGroupName)..."
            try {
                & $PsExe -NoProfile -File $Teardown -ResourceGroupName $M.ResourceGroupName 2>&1 | ForEach-Object { Show $_ }
                if ($LASTEXITCODE -ne 0) { Show "WARN: teardown exited $LASTEXITCODE" }
            }
            catch {
                Show "WARN: teardown error: $($_.Exception.Message)"
            }
            # Tidy up the per-machine creds file (never the shared .vm.pwd).
            if ((Split-Path $M.PwdFile -Leaf) -ne ".vm.pwd") {
                Remove-Item $M.PwdFile -ErrorAction SilentlyContinue
            }
        }
    }
    [pscustomobject]@{ Machine = $M.Label; Result = $status; Detail = $detail }
}

# --- Run ---------------------------------------------------------------------
$results = [System.Collections.Generic.List[object]]::new()

if ($Parallel) {
    # Log in once up front so N concurrent setups don't each launch a device-code
    # login. connect-to-azure.ps1 is idempotent (skips if already connected).
    Write-Host "● Ensuring Azure login before fan-out..."
    & $psExe -NoProfile -File (Join-Path $scriptDir "connect-to-azure.ps1")
    if ($StopOnFirstFailure) { Write-Warning "-StopOnFirstFailure is ignored with -Parallel (machines are already all in flight)." }

    # Re-create Invoke-Machine inside each parallel runspace from its source text
    # (script-scope functions aren't visible in -Parallel otherwise). Each
    # runspace is permissive and wraps the call so a single machine can never
    # abort the whole fan-out; the pipeline always returns one result per machine.
    $funcDef = ${function:Invoke-Machine}.ToString()
    try {
        # Stream each machine's result into $results as it completes, so an
        # unexpected late error can't discard results already gathered.
        $machines | ForEach-Object -ThrottleLimit $MaxParallel -Parallel {
            $ErrorActionPreference = 'Continue'
            ${function:Invoke-Machine} = $using:funcDef
            $machine = $_
            try {
                Invoke-Machine -M $machine -PsExe $using:psExe -RunTests $using:runTests -Teardown $using:teardown -Keep $using:keep
            }
            catch {
                [pscustomobject]@{ Machine = $machine.Label; Result = 'ERROR'; Detail = "$($_.Exception.Message)" }
            }
        } | ForEach-Object { $results.Add($_) }
    }
    catch {
        Write-Warning "Parallel run raised an unexpected error: $($_.Exception.Message)"
    }
}
else {
    foreach ($m in $machines) {
        $results.Add((Invoke-Machine -M $m -PsExe $psExe -RunTests $runTests -Teardown $teardown -Keep $keep))
        if ($StopOnFirstFailure -and $results[$results.Count - 1].Result -ne "PASSED") {
            Write-Warning "Stopping after first failure (-StopOnFirstFailure)."
            break
        }
    }
}

# --- Summary -----------------------------------------------------------------
Write-Host "`n===== SUMMARY ====="
$results | Sort-Object Machine | Format-Table -AutoSize Machine, Result, Detail | Out-String | Write-Host

$failed = @($results | Where-Object { $_.Result -ne "PASSED" })
if ($failed.Count -gt 0) {
    Write-Error "❌ $($failed.Count)/$($results.Count) machine(s) failed."
    exit 1
}
Write-Host "✅ All $($results.Count) machine(s) passed."
