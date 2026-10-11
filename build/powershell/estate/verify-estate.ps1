<#
.SYNOPSIS
    The estate gate: wait until every machine in a manifest is in sync with its
    role bundle and every one of its role checks returns what the role says.
.DESCRIPTION
    Polls, up to -WaitMinutes, every machine that provisioned cleanly:

        1. sync   GET /api/hosts/<id>/desired on the fleet server: the desired
                  state carries the role's bundle (role-<name>) and the host
                  reports it applied (in_sync against *that* state, not just
                  whatever it last synced).
        2. check  log in to the agent over REST (the web URL and password
                  from its .vm.<name>.pwd) and run each role check
                  that applies to its OS family; every check must return its
                  "expect" state with its "perf" keys, in the same round.

    A machine passes once both hold; the gate stops polling it then. Checks
    are re-run every round, so a collector warming up or a bundle still on its
    way is waited out rather than failed.

    For every machine it writes <RunFolder>/<machine>/checks.json: the sync
    state and, per check, the verdict and the raw REST result of the last
    attempt. -ResultFile gets one line per machine (Machine, Passed, Stage,
    Detail) for run-suite.ps1, where Stage is "sync" or "check".

    Exits 1 if any machine failed.
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
    How long to keep polling before failing what is still pending (default 10).
.PARAMETER RunFolder
    Where the per-machine checks.json files go. Default: a new
    .estate-runs/<timestamp> folder.
.PARAMETER ResultFile
    Optional JSON summary, one entry per machine.
.PARAMETER Name
    Only these machines (default: every machine in the manifest).
.PARAMETER Jest
    Once the gate has run, also run tests/live/estate.test.ts against the same
    manifest for a jest-shaped record (needs node and `npm install` in tests/).
    Its result does not change this script's verdicts or exit code.
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
    [switch]$Jest
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
        Checks     = @()
        BaseUrl    = $null
        Password   = $null
        Headers    = $null
        Synced     = $false
        SyncDetail = "not checked yet"
        Results    = @()
        Done       = $false
        Passed     = $false
        Stage      = $null
        Detail     = $null
    }
    if (-not $t.Role) { $t.Role = "base" }
    if (-not $catalog.Contains($t.Role)) {
        $t.Done = $true; $t.Stage = "check"; $t.Detail = "role '$($t.Role)' is not in $RolesDir"
    }
    else {
        $role = $catalog[$t.Role]
        if ($role.Bundle) { $t.Bundle = "role-$($role.Name)" }
        $t.Checks = @($role.Checks | Where-Object { Test-EstateCheckApplies -Check $_ -Family $t.Family })
        $creds = $null
        if ($m.PwdFile) { $creds = Read-EstatePwdFile -Path $m.PwdFile }
        # The pwd file's own "Web URL" first: it is what the setup script
        # actually configured.
        $url = $null
        if ($creds -and $creds.WebUrl) { $url = "$($creds.WebUrl)" }
        elseif ($m.PublicIp) { $url = "https://$($m.PublicIp):8443" }
        elseif ($creds -and $creds.PublicIp) { $url = "https://$($creds.PublicIp):8443" }
        if ($t.Checks.Count -gt 0) {
            if (-not $url -or -not $creds -or -not $creds.WebPassword) {
                $t.Done = $true; $t.Stage = "check"
                $t.Detail = "no web address or password (is $($m.PwdFile) still there?)"
            }
            else {
                $t.BaseUrl = $url
                $t.Password = $creds.WebPassword
            }
        }
    }
    if (-not $fleetGate) { $t.Synced = $true; $t.SyncDetail = "skipped (no fleet server or API key)" }
    elseif (-not $t.HostId) { $t.Synced = $false; $t.Done = $true; $t.Stage = "sync"; $t.Detail = "no fleet host id in the manifest" }
    $t
}
$targets = @($targets)

# --- one round -------------------------------------------------------------------
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

function Update-CheckState {
    param($T)
    if (-not $T.Headers) {
        try { $T.Headers = Connect-EstateAgent -BaseUrl $T.BaseUrl -Password $T.Password }
        catch {
            $T.Detail = "REST login to $($T.BaseUrl) failed: $($_.Exception.Message -split "`n" | Select-Object -First 1)"
            return $false
        }
    }
    $T.Results = @(foreach ($check in $T.Checks) {
            $raw = $null
            try {
                $raw = Invoke-EstateAgentCheck -BaseUrl $T.BaseUrl -Headers $T.Headers -Check $check
                $verdict = Test-EstateCheckResult -Check $check -Result $raw
            }
            catch {
                $err = $_.Exception.Message
                # A restarted agent (a bundle reload restarts the web server)
                # forgets its sessions: log in again next round.
                if ($err -match 'HTTP 40[13]') { $T.Headers = $null }
                $verdict = [pscustomobject]@{ Passed = $false; State = $null; Message = ""; PerfKeys = @(); Reason = "request failed: $($err -split "`n" | Select-Object -First 1)" }
            }
            [pscustomobject]@{
                Check    = $check.Label
                Command  = $check.Command
                Args     = $check.Args
                Expect   = $check.Expect
                Perf     = $check.Perf
                Passed   = $verdict.Passed
                State    = $verdict.State
                Reason   = $verdict.Reason
                Message  = $verdict.Message
                PerfKeys = $verdict.PerfKeys
                Response = $raw
            }
        })
    $failed = @($T.Results | Where-Object { -not $_.Passed })
    if ($failed.Count -eq 0) { $T.Detail = $null; return $true }
    $T.Detail = "$($failed.Count) of $($T.Results.Count) check(s) failing; first: $($failed[0].Check): $($failed[0].Reason)"
    return $false
}

$deadline = (Get-Date).AddMinutes($WaitMinutes)
Write-Host "Verifying $($targets.Count) machine(s), up to $WaitMinutes minute(s)..."
while ($true) {
    foreach ($t in @($targets | Where-Object { -not $_.Done })) {
        if (-not $t.Synced) { Update-SyncState -T $t }
        if (-not $t.Synced) { continue }
        if ($t.Checks.Count -eq 0 -or (Update-CheckState -T $t)) {
            $t.Done = $true; $t.Passed = $true
            Write-Host ("   {0}: PASSED ({1}; {2} check(s))" -f $t.Machine, $t.SyncDetail, $t.Checks.Count)
        }
    }
    $pending = @($targets | Where-Object { -not $_.Done })
    if ($pending.Count -eq 0 -or (Get-Date) -ge $deadline) { break }
    foreach ($t in $pending) {
        $why = if (-not $t.Synced) { "sync: $($t.SyncDetail)" } else { "check: $($t.Detail)" }
        Write-Host ("   [{0:HH:mm:ss}] {1} waiting - {2}" -f (Get-Date), $t.Machine, $why)
    }
    Start-Sleep -Seconds $PollSeconds
}

# --- verdicts ---------------------------------------------------------------------
foreach ($t in @($targets | Where-Object { -not $_.Passed -and -not $_.Stage })) {
    if (-not $t.Synced) { $t.Stage = "sync"; $t.Detail = $t.SyncDetail }
    else { $t.Stage = "check" }
}

foreach ($t in $targets) {
    $dir = Join-Path $RunFolder $t.Machine
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $record = [ordered]@{
        machine    = $t.Machine
        role       = $t.Role
        family     = $t.Family
        url        = $t.BaseUrl
        passed     = $t.Passed
        stage      = $t.Stage
        detail     = $t.Detail
        synced     = $t.Synced
        syncDetail = $t.SyncDetail
        checks     = @($t.Results)
    }
    ConvertTo-Json -InputObject $record -Depth 12 | Set-Content -Path (Join-Path $dir "checks.json") -Encoding UTF8
}

$summary = @($targets | ForEach-Object {
        [pscustomobject]@{ Machine = $_.Machine; Passed = $_.Passed; Stage = $_.Stage; Detail = $_.Detail }
    })
if ($ResultFile) { ConvertTo-Json -InputObject $summary -Depth 4 | Set-Content -Path $ResultFile -Encoding UTF8 }

Write-Host ""
foreach ($t in $targets) {
    if ($t.Passed) { Write-Host "$($t.Machine): PASSED" }
    else { Write-Host "$($t.Machine): FAILED at $($t.Stage) - $($t.Detail)" }
    foreach ($r in $t.Results) {
        $mark = if ($r.Passed) { "ok  " } else { "FAIL" }
        $line = "   $mark $($r.Check): $($r.State)"
        if (-not $r.Passed) {
            $line += " - $($r.Reason)"
            if ($r.Message) { $line += " :: $($r.Message)" }
        }
        Write-Host $line
    }
}
Write-Host "checks.json per machine under $RunFolder"

if ($Jest) {
    $testsDir = Join-Path (Split-Path -Parent (Split-Path -Parent $parentDir)) "tests"
    Write-Host ""
    Write-Host "Running tests/live/estate.test.ts against the same manifest..."
    $env:NSCP_ESTATE_MANIFEST = $ManifestFile
    $env:NSCP_ESTATE_ROLES = (Resolve-Path $RolesDir).Path
    Push-Location $testsDir
    try {
        $ErrorActionPreference = "Continue"
        # Piped through ForEach-Object, jest sees no terminal and drops its
        # colours; --colors and FORCE_COLOR (for anything chalk-based in the
        # tests) put them back, and Write-Host passes the escapes through to a
        # terminal that renders them (CLion's, Windows Terminal, pwsh). A
        # NO_COLOR in the environment is someone's choice, so it wins.
        $jestArgs = @("estate")
        $forceColor = -not $env:NO_COLOR
        $hadForceColor = [bool]$env:FORCE_COLOR
        if ($forceColor) {
            $jestArgs = @("--colors") + $jestArgs
            if (-not $hadForceColor) { $env:FORCE_COLOR = "1" }
        }
        try { & npm run test:live -- @jestArgs 2>&1 | ForEach-Object { Write-Host $_ } }
        finally { if ($forceColor -and -not $hadForceColor) { $env:FORCE_COLOR = $null } }
        if ($LASTEXITCODE -ne 0) { Write-Warning "estate.test.ts failed (exit $LASTEXITCODE); the gate's verdicts above stand." }
    }
    finally { Pop-Location }
}

if (@($targets | Where-Object { -not $_.Passed }).Count -gt 0) { exit 1 }
exit 0
