<#
.SYNOPSIS
    The estate gate: wait until every machine in a manifest is in sync with its
    role bundle, then run its checks through jest.
.DESCRIPTION
    Two stages, for every machine that provisioned cleanly:

        1. sync   polls GET /api/hosts/<id>/desired on the fleet server, up to
                  -WaitMinutes, until the desired state carries the role's
                  bundle (role-<name>-<estate id>) and the host reports it applied
                  (in_sync against *that* state, not just whatever it last
                  synced).
        2. check  runs tests/live/estate.test.ts (`npm run test:estate`) once
                  for every machine that synced: it logs in to each agent over
                  REST and runs the role checks that apply to its OS family
                  and the agent checks every live target must pass,
                  retrying each for -CheckWaitSeconds. The jest suite is the
                  only check runner; this script reads its verdicts.

    Jest writes <RunFolder>/<machine>/checks.json (per check the verdict, the
    reason and the raw REST result of the last attempt); this script adds the
    sync state to it, and writes one for a machine that never synced.
    -ResultFile gets one line per machine (Machine, Passed, Stage, Detail) for
    run-suite.ps1, where Stage is "sync" or "check".

    Needs node and `npm install` in tests/. Exits 1 if any machine failed.
.PARAMETER ManifestFile
    The estate manifest (default: build/powershell/.fleet-machines.json).
.PARAMETER FleetServer
    Fleet server url. Defaults to the manifest's "FleetServer".
.PARAMETER ApiKey
    Fleet API key for the sync stage (view_only is enough). Defaults to
    $env:NSCLIENT_FLEET_API_KEY. Without one the sync stage is skipped with a
    warning and only the checks are run.
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate.
.PARAMETER WaitMinutes
    How long to wait for the sync stage before failing what is still pending
    (default 10).
.PARAMETER CheckWaitSeconds
    How long jest retries one check before failing it (default 120): a
    collector warming up or the web server restarting after the bundle reload
    is waited out.
.PARAMETER CheckNsclient
    Also run every role check through the check_nsclient plugin (the binary a
    Nagios server runs), logged in to one profile per machine. Its exit code
    is the state, judged like the REST result.
.PARAMETER CheckNsclientPath
    The plugin to use. Default: check_nsclient on PATH, else the copy an
    NSClient++ install puts in its program folder.
.PARAMETER RunFolder
    Where the per-machine checks.json files go. Default: a new
    .estate-runs/<timestamp> folder.
.PARAMETER ResultFile
    Optional JSON summary, one entry per machine.
.PARAMETER Name
    Only these machines (default: every machine in the manifest).
.EXAMPLE
    ./estate/verify-estate.ps1 -ManifestFile .estate-runs/20261003-101500/suite-mysql/manifest.json
#>
param(
    [string]$ManifestFile = "",
    [string]$FleetServer = "",
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [switch]$FleetNoVerify,
    [ValidateRange(1, 240)]
    [int]$WaitMinutes = 10,
    [string]$RunFolder = "",
    [string]$ResultFile = "",
    [string[]]$Name = @(),
    [string]$RolesDir = "",
    [ValidateRange(5, 600)]
    [int]$PollSeconds = 20,
    [ValidateRange(0, 3600)]
    [int]$CheckWaitSeconds = 120,
    [switch]$CheckNsclient,
    [string]$CheckNsclientPath = ""
)

$ErrorActionPreference = "Stop"
$parentDir = Split-Path -Parent $PSScriptRoot
. (Join-Path $parentDir "fleet-api.ps1")
. (Join-Path $PSScriptRoot "estate-api.ps1")

if (-not $ManifestFile) { $ManifestFile = Join-Path $parentDir ".fleet-machines.json" }
$ManifestFile = (Resolve-Path $ManifestFile).Path
$machines = @(Read-FleetManifest -Path $ManifestFile)
# "a,b" arrives as one string through -File; accept both spellings.
$Name = @($Name | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Name.Count -gt 0) { $machines = @($machines | Where-Object { $Name -contains $_.Name }) }
# Only what provisioned cleanly: run-suite.ps1 has already given the rest
# their verdict, and a machine that never installed has nothing to check.
$machines = @($machines | Where-Object { -not $_.Status -or $_.Status -eq "OK" })
if ($machines.Count -eq 0) {
    Write-Host "Nothing to verify: no provisioned machines in $ManifestFile."
    if ($ResultFile) { Set-Content -Path $ResultFile -Value "[]" }
    return
}
if (-not $RolesDir) { $RolesDir = Get-EstateRolesDir }
$catalog = Get-EstateRoleCatalog -RolesDir $RolesDir
if (-not $RunFolder) { $RunFolder = New-EstateRunFolder }
if (-not $FleetServer) { $FleetServer = "$(@($machines | Where-Object { $_.FleetServer } | Select-Object -First 1).FleetServer)" }
$fleetGate = [bool]($FleetServer -and $ApiKey)
if (-not $fleetGate) {
    Write-Warning "No fleet server or API key: skipping the sync stage, running the checks only."
}


# --- what each machine has to satisfy ---------------------------------------------
$targets = foreach ($m in $machines) {
    $t = [pscustomobject]@{
        Machine    = $m.Name
        Role       = "$($m.Role)"
        Family     = "$(if ($m.Family) { $m.Family } else { $m.Os })"
        HostId     = "$($m.HostId)"
        Bundle     = $null
        Synced     = $false
        SyncDetail = "not checked yet"
        Done       = $false
        Passed     = $false
        Stage      = $null
        Detail     = $null
        Record     = $null
    }
    if (-not $t.Role) { $t.Role = "base" }
    if (-not $catalog.Contains($t.Role)) {
        $t.Done = $true; $t.Stage = "check"; $t.Detail = "role '$($t.Role)' is not in $RolesDir"
    }
    else {
        $role = $catalog[$t.Role]
        if ($role.Bundle) { $t.Bundle = Get-EstateGroupName -RoleName $role.Name -EstateId "$($m.EstateId)" }
        # jest only sees a machine whose credentials file is still there.
        if (-not ($m.PwdFile -and (Test-Path $m.PwdFile))) {
            $t.Done = $true; $t.Stage = "check"
            $t.Detail = "no web address or password (is $($m.PwdFile) still there?)"
        }
    }
    if (-not $fleetGate) { $t.Synced = $true; $t.SyncDetail = "skipped (no fleet server or API key)" }
    elseif (-not $t.HostId) { $t.Synced = $false; $t.Done = $true; $t.Stage = "sync"; $t.Detail = "no fleet host id in the manifest" }
    $t
}
$targets = @($targets)

# --- stage 1: sync -----------------------------------------------------------------
function Update-SyncState {
    param($T)
    try {
        $desired = Invoke-FleetApi -FleetServer $FleetServer -ApiKey $ApiKey -Path "/api/hosts/$($T.HostId)/desired" `
            -SkipCertificateCheck:$FleetNoVerify
    }
    catch {
        $T.SyncDetail = "could not read the desired state: $($_.Exception.Message -split "`n" | Select-Object -First 1)"
        return
    }
    $bundles = @($desired.bundles | ForEach-Object { "$($_.name)" })
    if ($T.Bundle -and $bundles -notcontains $T.Bundle) {
        $T.SyncDetail = "the desired state does not carry $($T.Bundle) (it has: $(if ($bundles) { $bundles -join ', ' } else { 'no bundles' })); is the host tagged for the role's group?"
        return
    }
    if (-not $desired.in_sync) {
        $T.SyncDetail = "not in sync with the desired state yet ($($bundles -join ', '))"
        return
    }
    $T.Synced = $true
    $T.SyncDetail = "in sync$(if ($bundles) { " with $($bundles -join ', ')" })"
}

$deadline = (Get-Date).AddMinutes($WaitMinutes)
Write-Host "Waiting for $($targets.Count) machine(s) to sync, up to $WaitMinutes minute(s)..."
while ($true) {
    foreach ($t in @($targets | Where-Object { -not $_.Done -and -not $_.Synced })) {
        Update-SyncState -T $t
        if ($t.Synced) { Write-Host ("   {0}: {1}" -f $t.Machine, $t.SyncDetail) }
    }
    $pending = @($targets | Where-Object { -not $_.Done -and -not $_.Synced })
    if ($pending.Count -eq 0 -or (Get-Date) -ge $deadline) { break }
    foreach ($t in $pending) {
        Write-Host ("   [{0:HH:mm:ss}] {1} waiting - sync: {2}" -f (Get-Date), $t.Machine, $t.SyncDetail)
    }
    Start-Sleep -Seconds $PollSeconds
}
foreach ($t in @($targets | Where-Object { -not $_.Done -and -not $_.Synced })) {
    $t.Done = $true; $t.Stage = "sync"; $t.Detail = $t.SyncDetail
}

# --- stage 2: checks, through jest -----------------------------------------------
$toCheck = @($targets | Where-Object { -not $_.Done })
if ($toCheck.Count -gt 0) {
    $testsDir = Join-Path (Split-Path -Parent (Split-Path -Parent $parentDir)) "tests"
    $jestProblem = $null
    $checkNsclientExe = ""
    if ($CheckNsclient -or $CheckNsclientPath) {
        $candidates = @($CheckNsclientPath)
        if (-not $CheckNsclientPath) {
            $candidates = @((Get-Command check_nsclient -ErrorAction SilentlyContinue).Source)
            if ($env:ProgramFiles) { $candidates += Join-Path $env:ProgramFiles "NSClient++\check_nsclient.exe" }
        }
        $checkNsclientExe = @($candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1)[0]
        if (-not $checkNsclientExe) {
            $jestProblem = "-CheckNsclient: no check_nsclient plugin found (looked for $(@($candidates | Where-Object { $_ }) -join ', '); pass -CheckNsclientPath)"
        }
        else { Write-Host "Role checks also run through $checkNsclientExe" }
    }
    if (-not (Get-Command npm -ErrorAction SilentlyContinue)) { $jestProblem = "npm is not on PATH (the checks run in jest)" }
    elseif (-not (Test-Path (Join-Path $testsDir "node_modules"))) { $jestProblem = "tests/node_modules is missing: run 'npm install' in $testsDir" }

    $rc = $null
    if (-not $jestProblem) {
        # A previous run's file must not pass for this one's.
        foreach ($t in $toCheck) { Remove-Item (Join-Path (Join-Path $RunFolder $t.Machine) "checks.json") -ErrorAction SilentlyContinue }
        Write-Host ""
        Write-Host "Running the checks of $($toCheck.Count) machine(s) (tests/live/estate.test.ts)..."
        $saved = @{}
        $jestEnv = @{
            NSCP_ESTATE_MANIFEST           = $ManifestFile
            NSCP_ESTATE_ROLES              = (Resolve-Path $RolesDir).Path
            NSCP_ESTATE_MACHINES           = (($toCheck | ForEach-Object { $_.Machine }) -join ",")
            NSCP_ESTATE_RESULTS_DIR        = (Resolve-Path $RunFolder).Path
            NSCP_ESTATE_CHECK_WAIT_SECONDS = "$CheckWaitSeconds"
            NSCP_ESTATE_CHECK_NSCLIENT     = $checkNsclientExe
        }
        # Piped through ForEach-Object, jest sees no terminal and drops its
        # colours; --colors and FORCE_COLOR (for anything chalk-based in the
        # tests) put them back, and Write-Host passes the escapes through to a
        # terminal that renders them (CLion's, Windows Terminal, pwsh). A
        # NO_COLOR in the environment is someone's choice, so it wins.
        $jestArgs = @()
        if (-not $env:NO_COLOR) {
            $jestArgs = @("--colors")
            if (-not $env:FORCE_COLOR) { $jestEnv.FORCE_COLOR = "1" }
        }
        foreach ($k in $jestEnv.Keys) {
            $saved[$k] = [Environment]::GetEnvironmentVariable($k)
            Set-Item -Path "env:$k" -Value $jestEnv[$k]
        }
        Push-Location $testsDir
        $eap = $ErrorActionPreference
        try {
            # npm writes its banner to stderr; that is not a failure.
            $ErrorActionPreference = "Continue"
            & npm run test:estate -- @jestArgs 2>&1 | ForEach-Object { Write-Host $_ }
            $rc = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $eap
            Pop-Location
            foreach ($k in $saved.Keys) {
                if ($null -eq $saved[$k]) { Remove-Item -Path "env:$k" -ErrorAction SilentlyContinue }
                else { Set-Item -Path "env:$k" -Value $saved[$k] }
            }
        }
    }

    foreach ($t in $toCheck) {
        $t.Done = $true; $t.Stage = "check"
        $file = Join-Path (Join-Path $RunFolder $t.Machine) "checks.json"
        if ($jestProblem) { $t.Detail = $jestProblem; continue }
        if (-not (Test-Path $file)) {
            $t.Detail = "the jest suite wrote no checks.json for it (jest exit $rc; see the output above)"
            continue
        }
        $t.Record = Get-Content -Path $file -Raw | ConvertFrom-Json
        $t.Passed = [bool]$t.Record.passed
        if ($t.Passed) { $t.Stage = $null } else { $t.Detail = "$($t.Record.detail)" }
    }
}

# --- verdicts ---------------------------------------------------------------------
foreach ($t in $targets) {
    $dir = Join-Path $RunFolder $t.Machine
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $checks = @()
    $url = $null
    if ($t.Record) { $checks = @($t.Record.checks); $url = $t.Record.url }
    $record = [ordered]@{
        machine    = $t.Machine
        role       = $t.Role
        family     = $t.Family
        url        = $url
        passed     = $t.Passed
        stage      = $t.Stage
        detail     = $t.Detail
        synced     = $t.Synced
        syncDetail = $t.SyncDetail
        checks     = $checks
    }
    ConvertTo-Json -InputObject $record -Depth 12 | Set-Content -Path (Join-Path $dir "checks.json") -Encoding UTF8
}

$summary = @($targets | ForEach-Object {
        [pscustomobject]@{ Machine = $_.Machine; Passed = $_.Passed; Stage = $_.Stage; Detail = $_.Detail }
    })
if ($ResultFile) { ConvertTo-Json -InputObject $summary -Depth 4 | Set-Content -Path $ResultFile -Encoding UTF8 }

Write-Host ""
foreach ($t in $targets) {
    if ($t.Passed) { Write-Host "$($t.Machine): PASSED ($($t.SyncDetail); $(@($t.Record.checks).Count) check(s))" }
    else { Write-Host "$($t.Machine): FAILED at $($t.Stage) - $($t.Detail)" }
}
Write-Host "checks.json per machine under $RunFolder"

if (@($targets | Where-Object { -not $_.Passed }).Count -gt 0) { exit 1 }
exit 0
