<#
.SYNOPSIS
    One command per estate suite: provision the machines a role needs, verify
    them against the fleet server, collect their logs and tear them down.
.DESCRIPTION
    A suite is a role from estate/roles/ (see README.md, "Role-based estate").
    For each suite, estate/run-suite.ps1

        1. provisions the role's machines (provision-fleet-machines.ps1 -Roles),
           which installs the workload, NSClient++ with the role's modules, and
           enrolls and tags each machine with the fleet server;
        2. waits until each one has enrolled and reports in sync;
        3. composes the role's bundle (estate/add-role-bundles.ps1) and gates
           on it (estate/verify-estate.ps1): each machine must be in sync with
           the bundle and pass every role check, run by the jest suite
           tests/live/estate.test.ts (needs `npm install` in tests/);
        4. collects every machine's logs into .estate-runs/<timestamp>/<machine>/,
           on success and failure alike;
        5. tears the machines down (unless -KeepVms).

    At the end it prints the run folder and one verdict per machine; a failed
    one names the first stage that failed (provision, install, enroll, sync,
    bundle, check), so the right log is the first one opened. Then how long
    each suite spent in each stage (from its timing.json) and the total
    runtime. The exit code is non-zero if any suite failed.

    Suites run concurrently by default, each as its own process (so this works
    on Windows PowerShell 5.1 too), within -MaxParallel and -MaxMachines.
    -MaxMachines caps how many machines are up at once, so a run never trips
    the vCPU quota half way through: suites start only while the machines in
    flight stay within the cap, and the rest wait their turn. -Sequential runs
    them one after another instead.
.PARAMETER Suite
    The suites to run: role names, comma-separated ("mysql,mssql"), optionally
    with a machine count ("base=3"). "all" is every role in the catalog.
.PARAMETER Sequential
    Run the suites one at a time, in the order given, instead of concurrently.
    Each machine's credentials are then also copied to the shared .vm.pwd, so
    connect-machine.ps1 and `npm run test:live` (tests/) find the latest one, and
    -StopOnFirstFailure applies.
.PARAMETER MaxParallel
    How many suites run at once (default 4).
.PARAMETER MaxMachines
    How many machines may be up at once (default 5). Raise it only when the
    regional vCPU quota allows it.
.PARAMETER KeepVms
    Leave the machines up after the run (the logs are still collected). Each
    suite prints the -Destroy command for its manifest.
.PARAMETER StopOnFirstFailure
    Stop after the first failed suite. Only with -Sequential.
.PARAMETER Collect
    Do nothing but pull logs for the machines in -ManifestFile (default: the
    .fleet-machines.json estate) into a new run folder.
.PARAMETER Cleanup
    Do nothing but tear down everything the estate tooling left behind: every
    machine in every manifest (.fleet-machines.json and every run's
    suite-<role>/manifest.json), every stray NSCP-Fleet-* resource group in
    Azure that no manifest lists, their fleet hosts, and then the run folders
    and .vm.<name>.pwd files nothing live is left in. Asks first unless
    -Force; -DryRun shows the plan; -KeepLogs keeps the run folders.
    (estate/cleanup-estate.ps1)
.PARAMETER KeepLogs
    With -Cleanup: keep the run folders under .estate-runs.
.PARAMETER Force
    With -Cleanup: do not ask before deleting.
.PARAMETER DryRun
    Print the suites, their machines and the order they would run in, and stop.
    Touches neither Azure nor the fleet server.
.PARAMETER WaitMinutes
    How long a suite waits for its machines to enroll and sync (default 15).
.PARAMETER CheckNsclient
    Also run every role check through the check_nsclient plugin from this
    machine, the way a Nagios server would (see estate/verify-estate.ps1).
.EXAMPLE
    ./run-estate.ps1 base                            # today's estate, plus log collection
.EXAMPLE
    ./run-estate.ps1 mysql,mssql,iis                 # three suites, concurrently
.EXAMPLE
    ./run-estate.ps1 all -MaxMachines 8              # everything, as the quota allows
.EXAMPLE
    ./run-estate.ps1 mysql,mssql -Sequential -StopOnFirstFailure
.EXAMPLE
    ./run-estate.ps1 mssql -KeepVms                  # leave it up, then connect-machine.ps1
.EXAMPLE
    ./run-estate.ps1 -Collect                        # just pull logs from the current manifest
.EXAMPLE
    ./run-estate.ps1 -Cleanup -DryRun                # what a cleanup would remove
.EXAMPLE
    ./run-estate.ps1 -Cleanup -Force                 # remove it all, without asking
#>
param(
    [Parameter(Position = 0)]
    [string[]]$Suite = @(),
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [switch]$Sequential,
    [ValidateRange(1, 16)]
    [int]$MaxParallel = 4,
    [ValidateRange(1, 100)]
    [int]$MaxMachines = 5,
    [switch]$KeepVms,
    [switch]$StopOnFirstFailure,
    [switch]$Collect,
    [switch]$Cleanup,
    [switch]$KeepLogs,
    [switch]$Force,
    [string]$ManifestFile = "",
    [switch]$DryRun,
    [ValidateRange(1, 120)]
    [int]$WaitMinutes = 15,
    [switch]$CheckNsclient,
    [string]$Version = "",
    [string]$WindowsPackageUrl = "",
    [string]$UbuntuPackageUrl = "",
    [string]$RockyPackageUrl = "",
    [string]$Location = "",
    [string[]]$WindowsVersions = @(),
    [switch]$FleetInsecure,
    [switch]$FleetNoVerify,
    [string]$FleetCaFile = "",
    [string]$RolesDir = ""
)

$ErrorActionPreference = "Stop"
$runStart = Get-Date
$scriptDir = $PSScriptRoot
$estateDir = Join-Path $scriptDir "estate"
. (Join-Path $scriptDir "fleet-api.ps1")
. (Join-Path $estateDir "estate-api.ps1")
$psExe = (Get-Process -Id $PID).Path
$Parallel = -not $Sequential

# --- -Collect: logs only -----------------------------------------------------
if ($Collect) {
    $collectArgs = @{}
    if ($ManifestFile) { $collectArgs.ManifestFile = $ManifestFile }
    if ($FleetServer) { $collectArgs.FleetServer = $FleetServer }
    if ($FleetNoVerify) { $collectArgs.FleetNoVerify = $true }
    & (Join-Path $estateDir "collect-logs.ps1") @collectArgs
    return
}

# --- -Cleanup: tear down everything ever left behind --------------------------
if ($Cleanup) {
    $cleanupArgs = @{ KeepLogs = $KeepLogs; DryRun = $DryRun; Force = $Force; FleetNoVerify = $FleetNoVerify }
    if ($FleetServer) { $cleanupArgs.FleetServer = $FleetServer }
    if ($ApiKey) { $cleanupArgs.ApiKey = $ApiKey }
    # A child process: the script calls `exit`. (Not $cleanup: variable names
    # are case-insensitive, so that is the -Cleanup switch itself.)
    $cleanupScript = Join-Path $estateDir "cleanup-estate.ps1"
    $argList = @()
    foreach ($k in $cleanupArgs.Keys) {
        $v = $cleanupArgs[$k]
        if ($v -is [switch] -or $v -is [bool]) { if ($v) { $argList += "-$k" } }
        else { $argList += @("-$k", "$v") }
    }
    & $psExe -NoProfile -File $cleanupScript @argList
    exit $LASTEXITCODE
}

# --- Resolve the suites ------------------------------------------------------
if (-not $RolesDir) { $RolesDir = Get-EstateRolesDir }
$catalog = Get-EstateRoleCatalog -RolesDir $RolesDir
# "mysql,mssql" arrives as an array when typed on the command line and as one
# string through -File; accept both.
$specs = @($Suite | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($specs.Count -eq 0) {
    throw "Name the suites to run, e.g. ./run-estate.ps1 base, or 'all'. Roles: $(@($catalog.Keys) -join ', ')."
}
if ($specs -contains "all") {
    # Every role, keeping an explicit count given next to it ("all,base=3").
    $named = @($specs | Where-Object { $_ -ne "all" })
    $mentioned = @($named | ForEach-Object { ($_ -split '=')[0] })
    # A companion-only role (a hub's targets) comes with the role that needs it.
    $specs = $named + @($catalog.Keys | Where-Object { $mentioned -notcontains $_ -and -not $catalog[$_].CompanionOnly })
}

$suites = [System.Collections.Generic.List[object]]::new()
foreach ($spec in $specs) {
    $machines = @(ConvertFrom-RoleSpec -Spec $spec -Catalog $catalog)
    $role = $machines[0].Role
    if (@($suites | Where-Object { $_.Role -eq $role.Name })) { throw "Suite '$($role.Name)' is named twice." }
    if ($machines.Count -gt $MaxMachines) {
        throw "Suite '$($role.Name)' needs $($machines.Count) machines, more than -MaxMachines $MaxMachines on its own."
    }
    # Spec is what the suite provisions (its companions come along); Count is
    # every machine that adds up to, which is what the budget counts.
    $own = @($machines | Where-Object { $_.Role.Name -eq $role.Name }).Count
    $suites.Add([pscustomobject]@{ Role = $role.Name; Spec = "$($role.Name)=$own"; Count = $machines.Count; Machines = $machines })
}
$total = ($suites | Measure-Object -Property Count -Sum).Sum

Write-Host "● Plan: $($suites.Count) suite(s), $total machine(s), at most $MaxMachines up at once"
foreach ($s in $suites) {
    $oses = ($s.Machines | ForEach-Object { $_.Os }) -join ", "
    Write-Host ("   - {0,-10} {1} machine(s): {2}" -f $s.Role, $s.Count, $oses)
}
if ($total -gt $MaxMachines) {
    if ($Parallel) {
        Write-Warning "$total machines is more than -MaxMachines ${MaxMachines}: suites will wait for earlier ones to tear down."
    }
}
if ($Parallel -and $StopOnFirstFailure) {
    Write-Warning "-StopOnFirstFailure only applies with -Sequential (concurrent suites are already in flight)."
}
if ($DryRun) {
    Write-Host "● -DryRun: nothing was created."
    return
}

if (-not $FleetServer) { throw "-FleetServer is required (or set `$env:NSCLIENT_FLEET_SERVER)." }
if (-not $ApiKey) { throw "-ApiKey is required (or set `$env:NSCLIENT_FLEET_API_KEY)." }
# Children inherit the key through the environment rather than a file or a
# command line.
$env:NSCLIENT_FLEET_API_KEY = $ApiKey

# Resolve "the latest release" once for the whole run: left to each suite it
# is one unauthenticated GitHub API call per suite (60/hour per address), and a
# release published mid-run would give earlier and later suites different
# versions. Only when no -*PackageUrl is given: an explicit -Version also
# turns the version check on for a package-url family, which must keep
# installing whatever its package holds.
if (-not $Version -and -not ($WindowsPackageUrl -or $UbuntuPackageUrl -or $RockyPackageUrl)) {
    $Version = Get-LatestNscpRelease
    Write-Host "● NSClient++ $Version for every suite (the latest GitHub release; -Version picks another)"
}

# Arguments every suite hands to provision-fleet-machines.ps1. -Version only
# when given or resolved above, so a -*PackageUrl build keeps skipping the
# version check.
$provisionArgs = @("-FleetServer", $FleetServer)
foreach ($p in @("Version", "WindowsPackageUrl", "UbuntuPackageUrl", "RockyPackageUrl", "Location", "FleetCaFile")) {
    $value = Get-Variable -Name $p -ValueOnly
    if ($value) { $provisionArgs += @("-$p", $value) }
}
# A list typed on the command line arrives as an array, through -File as one
# string; the suite hands it on as one comma-separated argument either way.
if ($WindowsVersions) { $provisionArgs += @("-WindowsVersions", ($WindowsVersions -join ",")) }
if ($FleetInsecure) { $provisionArgs += "-FleetInsecure" }
if ($FleetNoVerify) { $provisionArgs += "-FleetNoVerify" }
if ($RolesDir) { $provisionArgs += @("-RolesDir", $RolesDir) }
# Within a suite, its own machines provision concurrently where PowerShell can.
if ($Parallel -and $PSVersionTable.PSVersion.Major -ge 7) { $provisionArgs += "-Parallel" }

# Fail on an unreachable fleet server or a refused key now, not once per suite.
Test-FleetServer -FleetServer $FleetServer -SkipCertificateCheck:$FleetNoVerify | Out-Null
Test-FleetApiKey -FleetServer $FleetServer -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify
# Log in once up front, so concurrent suites do not each prompt.
& (Join-Path $scriptDir "connect-to-azure.ps1")

$runFolder = New-EstateRunFolder
Write-Host "● Run folder: $runFolder"
foreach ($s in $suites) {
    $dir = Join-Path $runFolder "suite-$($s.Role)"
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $sharedPwd = $null
    if (-not $Parallel) { $sharedPwd = Join-Path $scriptDir ".vm.pwd" }
    $config = [ordered]@{
        role          = $s.Role
        spec          = $s.Spec
        count         = $s.Count
        runFolder     = $runFolder
        provisionArgs = $provisionArgs
        fleetServer   = $FleetServer
        fleetNoVerify = [bool]$FleetNoVerify
        waitMinutes   = $WaitMinutes
        keepVms       = [bool]$KeepVms
        checkNsclient = [bool]$CheckNsclient
        sharedPwdFile = $sharedPwd
    }
    $s | Add-Member -NotePropertyName SuiteFile -NotePropertyValue (Join-Path $dir "suite.json")
    $config | ConvertTo-Json -Depth 4 | Set-Content -Path $s.SuiteFile
}

# --- Run -------------------------------------------------------------------------
$runSuite = Join-Path $estateDir "run-suite.ps1"
$started = [System.Collections.Generic.HashSet[string]]::new()
if ($Parallel) {
    # Each suite is a background job running its own process, so this works on
    # 5.1 as well. A suite starts only while the machines in flight stay
    # within -MaxMachines, which is what keeps `all` inside the quota.
    $queue = [System.Collections.Generic.Queue[object]]::new([object[]]$suites.ToArray())
    $running = [System.Collections.Generic.List[object]]::new()
    while ($queue.Count -gt 0 -or $running.Count -gt 0) {
        $inFlight = ($running | Measure-Object -Property Count -Sum).Sum
        while ($queue.Count -gt 0 -and $running.Count -lt $MaxParallel -and
            ($inFlight + $queue.Peek().Count) -le $MaxMachines) {
            $s = $queue.Dequeue()
            [void]$started.Add($s.Role)
            Write-Host "● Starting suite $($s.Role) ($($s.Count) machine(s))"
            $job = Start-Job -ScriptBlock {
                param($PsExe, $Script, $SuiteFile)
                & $PsExe -NoProfile -File $Script -SuiteFile $SuiteFile 2>&1 | ForEach-Object { "$_" }
            } -ArgumentList $psExe, $runSuite, $s.SuiteFile
            $running.Add([pscustomobject]@{ Suite = $s; Job = $job; Count = $s.Count })
            $inFlight += $s.Count
        }
        Start-Sleep -Seconds 5
        foreach ($r in @($running)) {
            Receive-Job -Job $r.Job -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
            if ($r.Job.State -ne "Running") {
                Receive-Job -Job $r.Job -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
                Remove-Job -Job $r.Job -Force
                [void]$running.Remove($r)
                Write-Host "● Suite $($r.Suite.Role) finished"
            }
        }
    }
}
else {
    foreach ($s in $suites) {
        Write-Host "● Suite $($s.Role) ($($s.Count) machine(s))"
        [void]$started.Add($s.Role)
        & $psExe -NoProfile -File $runSuite -SuiteFile $s.SuiteFile 2>&1 | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0 -and $StopOnFirstFailure) {
            Write-Warning "Stopping after the first failed suite (-StopOnFirstFailure)."
            break
        }
    }
}

# --- Verdict -------------------------------------------------------------------
$results = [System.Collections.Generic.List[object]]::new()
foreach ($s in $suites) {
    $resultFile = Join-Path (Split-Path -Parent $s.SuiteFile) "result.json"
    if (Test-Path $resultFile) {
        # ForEach-Object unrolls it: PS 5.1's ConvertFrom-Json emits a JSON array
        # as ONE object, which printed a two-machine suite as System.Object[].
        foreach ($r in @(Get-Content -Path $resultFile -Raw | ConvertFrom-Json | ForEach-Object { $_ })) { $results.Add($r) }
    }
    elseif ($started.Contains($s.Role)) {
        $results.Add([pscustomobject]@{ Machine = "(suite $($s.Role))"; Role = $s.Role; Verdict = "FAILED"; Stage = "provision"; Detail = "the suite wrote no result" })
    }
    else {
        Write-Host "   suite $($s.Role) was not run (-StopOnFirstFailure)"
    }
}

Write-Host "`n===== ESTATE RUN ====="
foreach ($r in $results) {
    if ($r.Verdict -eq "PASSED") { Write-Host ("   {0,-16} {1,-8} PASSED" -f $r.Machine, $r.Role) }
    else { Write-Host ("   {0,-16} {1,-8} FAILED at {2}: {3}" -f $r.Machine, $r.Role, $r.Stage, $r.Detail) -ForegroundColor Red }
}
Write-Host "Logs: $runFolder"

# Where the time went: each suite's stages (run-suite.ps1 writes timing.json),
# then the whole run, preflight and queueing included.
Write-Host "`n===== TIMING ====="
foreach ($s in $suites) {
    $timingFile = Join-Path (Split-Path -Parent $s.SuiteFile) "timing.json"
    if (-not (Test-Path $timingFile)) { continue }
    $t = Get-Content -Path $timingFile -Raw | ConvertFrom-Json
    $stages = @($t.stages.PSObject.Properties | ForEach-Object { "$($_.Name) $(Format-EstateDuration $_.Value)" }) -join ", "
    Write-Host ("   {0,-10} {1,-10} {2}" -f $s.Role, (Format-EstateDuration $t.seconds), $stages)
}
Write-Host "Total runtime: $(Format-EstateDuration ((Get-Date) - $runStart).TotalSeconds)"

$failed = @($results | Where-Object { $_.Verdict -ne "PASSED" })
if ($failed.Count -gt 0 -or $results.Count -eq 0) {
    Write-Host "❌ $($failed.Count)/$($results.Count) machine(s) failed." -ForegroundColor Red
    exit 1
}
Write-Host "✅ All $($results.Count) machine(s) passed."
