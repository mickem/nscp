<#
.SYNOPSIS
    Provision Windows and Linux VMs in Azure, install NSClient++ on them and
    enroll every one of them with an NSClient fleet server.
.DESCRIPTION
    A wrapper over the setup-*-machine.ps1 scripts in this directory that adds
    the two things a fleet demo/test estate needs: a bootstrap token per machine,
    and machines that look like a real estate rather than "vm-test-3".

    For each machine it

      1. invents a name - <role>-<site>-<nn>, e.g. web-ams-04, sql-fra-12 - which
         becomes the VM name, the computer name the fleet server sees, and the
         name of its own resource group;
      2. mints a one-time bootstrap token from the fleet server
         (POST /api/hosts with your API key), immediately before provisioning
         that machine, since the token expires in about an hour;
      3. runs the matching setup script, which provisions the VM, installs
         NSClient++ and enrolls it - through the MSI's FLEET_SERVER/FLEET_TOKEN
         properties on Windows (falling back to `nscp enroll` if the MSI predates
         them), and with `nscp enroll` on Linux.

    Every machine gets its own resource group and its own .vm.<name>.pwd
    credentials file, and is recorded in a manifest (.fleet-machines.json) so
    -Destroy can clean the whole estate up again.

    Requires the Az modules (./install-azure.ps1) and an API key from the fleet
    server's *API keys* page. Note that the VMs enroll from Azure, so the fleet
    server url has to be reachable from the internet - a localhost dev server
    will pass the preflight check here and then fail on the VM.
.PARAMETER FleetServer
    Fleet server url, e.g. https://fleet.example.com. Defaults to
    $env:NSCLIENT_FLEET_SERVER.
.PARAMETER ApiKey
    Fleet server API key ("nsk_..."), used to mint one bootstrap token per
    machine. Defaults to $env:NSCLIENT_FLEET_API_KEY. The key acts as its owner,
    so a key belonging to an add_hosts user is enough (and is the better choice).
.PARAMETER Windows
    How many Windows machines to create (default 1).
.PARAMETER Ubuntu
    How many Ubuntu machines to create (default 1).
.PARAMETER Rocky
    How many Rocky Linux machines to create (default 0).
.PARAMETER Version
    NSClient++ release to install on every machine. Ignored for a family that has
    its own -*PackageUrl unless you also pass -Version explicitly.
.PARAMETER WindowsPackageUrl
    Install the MSI from this url instead of the GitHub release. Needed to test
    the installer's own fleet enrollment before it ships in a release.
.PARAMETER UbuntuPackageUrl
    Install the DEB from this url instead of the GitHub release.
.PARAMETER RockyPackageUrl
    Install the RPM from this url instead of the GitHub release.
.PARAMETER FleetInsecure
    Allow an unauthenticated enrollment - needed for a plain http:// fleet server.
.PARAMETER FleetNoVerify
    Also skip verification of the fleet server certificate, here and on the
    machines (for a self-signed test server without -FleetCaFile). Implies
    -FleetInsecure.
.PARAMETER FleetCaFile
    Local PEM file with the CA that issued the fleet server certificate; it is
    copied to each machine and used to verify its enrollment call.
.PARAMETER Parallel
    Provision the machines concurrently (PowerShell 7+), -MaxParallel at a time.
.PARAMETER DryRun
    Print the machines that would be created (names included) and stop. Touches
    neither Azure nor the fleet server.
.PARAMETER Destroy
    Delete every machine in the manifest: its resource group, its credentials
    file and its manifest entry. Add -RemoveFleetHosts to also delete the hosts
    from the fleet server (needs an admin/owner API key).

    The resource groups are deleted concurrently (one takes minutes, and it is
    nearly all waiting on Azure), so a teardown costs about as long as its
    slowest machine rather than the sum of all of them.
.PARAMETER NoWait
    With -Destroy: start every resource-group deletion and return without
    waiting for them. Azure finishes them on its own. The manifest is kept,
    because nothing was confirmed gone - re-run -Destroy afterwards to verify
    and clear it (groups that are already gone drop out silently).
.EXAMPLE
    # One Windows + one Ubuntu machine, enrolled with the fleet server:
    $env:NSCLIENT_FLEET_API_KEY = "nsk_..."
    ./provision-fleet-machines.ps1 -FleetServer https://fleet.example.com
.EXAMPLE
    # A six-machine estate, concurrently, on an unreleased build:
    ./provision-fleet-machines.ps1 -FleetServer https://fleet.example.com `
        -Windows 3 -Ubuntu 2 -Rocky 1 -Parallel `
        -WindowsPackageUrl https://my.host/NSCP-0.16.0-x64.msi
.EXAMPLE
    # Tear the whole estate down again, fleet entries included:
    ./provision-fleet-machines.ps1 -Destroy -RemoveFleetHosts `
        -FleetServer https://fleet.example.com
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [ValidateRange(0, 50)]
    [int]$Windows = 1,
    [ValidateRange(0, 50)]
    [int]$Ubuntu = 1,
    [ValidateRange(0, 50)]
    [int]$Rocky = 0,
    [string]$Version = "0.17.0",
    [string]$Location = "WestEurope",
    [string[]]$WindowsVersions = @("windows-2025"),
    [string]$UbuntuVersion = "24.04",
    [string]$RockyVersion = "9",
    [string]$WindowsPackageUrl = "",
    [string]$UbuntuPackageUrl = "",
    [string]$RockyPackageUrl = "",
    [switch]$FleetInsecure,
    [switch]$FleetNoVerify,
    [string]$FleetCaFile = "",
    [string]$VmSize = "Standard_F1as_v7",
    [string]$ResourceGroupPrefix = "NSCP-Fleet",
    [string]$AdminUsername = "azureadmin",
    [switch]$Parallel,
    [ValidateRange(1, 16)]
    [int]$MaxParallel = 4,
    [string]$ManifestFile = "",
    [switch]$DryRun,
    [switch]$Destroy,
    [switch]$RemoveFleetHosts,
    [switch]$NoWait,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$fleetApi = Join-Path $scriptDir "fleet-api.ps1"
. $fleetApi

if (-not $ManifestFile) { $ManifestFile = Join-Path $scriptDir ".fleet-machines.json" }
# Skipping certificate validation for the API calls made from here has to follow
# the same switch the machines get, or we would happily mint a token against a
# server the machines then refuse to talk to (or the other way round).
$skipCert = [bool]$FleetNoVerify

function Read-Manifest {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return @() }
    try { return @(Get-Content -Path $Path -Raw | ConvertFrom-Json) }
    catch {
        Write-Warning "Could not read the manifest $Path ($($_.Exception.Message)); treating it as empty."
        return @()
    }
}

function Write-Manifest {
    param([string]$Path, $Machines)
    # ConvertTo-Json unrolls a single-element array into an object, which would
    # then read back as one machine's worth of properties - force an array.
    $json = ConvertTo-Json -InputObject @($Machines) -Depth 5
    if (@($Machines).Count -eq 1 -and -not $json.StartsWith("[")) { $json = "[$json]" }
    Set-Content -Path $Path -Value $json -Force
}

# --- Destroy mode ------------------------------------------------------------
if ($Destroy) {
    $existing = Read-Manifest -Path $ManifestFile
    if (@($existing).Count -eq 0) {
        Write-Host "Nothing to destroy: $ManifestFile is missing or empty."
        return
    }
    Write-Host "● The manifest lists $(@($existing).Count) machine(s):"
    $existing | ForEach-Object { Write-Host "   - $($_.Name)  ($($_.Os), RG $($_.ResourceGroup), fleet host $($_.HostId))" }
    if (-not $Force) {
        $answer = Read-Host "Delete these resource groups? [y/N]"
        if ($answer -notmatch '^(y|yes)$') { Write-Host "Aborted."; return }
    }

    & (Join-Path $scriptDir "connect-to-azure.ps1")
    if (-not (Get-Module -ListAvailable -Name Az.Resources)) {
        Install-Module -Name Az.Resources -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module Az.Resources
    $remaining = [System.Collections.Generic.List[object]]::new()

    # Fleet hosts first, sequentially: these are quick REST calls, and doing
    # them before the (slow) Azure deletes means a machine leaves the fleet UI
    # immediately rather than minutes later.
    foreach ($m in $existing) {
        if (-not $RemoveFleetHosts) { break }
        # The manifest remembers which server each machine enrolled with, so
        # -FleetServer only has to be passed to override it.
        $server = if ($FleetServer) { $FleetServer } else { $m.FleetServer }
        if (-not $m.HostId) {
            Write-Warning "   $($m.Name) has no fleet host id in the manifest; nothing to remove."
        }
        elseif (-not $server -or -not $ApiKey) {
            Write-Warning "   -RemoveFleetHosts needs a fleet server url and an API key; keeping fleet host $($m.HostId)."
        }
        else {
            try {
                Remove-FleetHost -FleetServer $server -ApiKey $ApiKey -HostId $m.HostId -SkipCertificateCheck:$skipCert
                Write-Host "   ✅ removed fleet host $($m.HostId) ($($m.Name))"
            }
            catch {
                # An add_hosts key cannot delete; that must not stop the teardown.
                Write-Warning "   could not remove fleet host $($m.HostId): $($_.Exception.Message)"
            }
        }
    }

    # Then the resource groups, all at once. Deleting one takes minutes and is
    # almost entirely waiting on Azure, so doing them one after another made a
    # teardown take (machines x minutes). -AsJob starts the delete and returns
    # immediately, which parallelises the waiting - and unlike
    # ForEach-Object -Parallel it needs no PowerShell 7.
    $pending = [System.Collections.Generic.List[object]]::new()
    foreach ($m in $existing) {
        try {
            # A resource group that is already gone (a previous partial teardown,
            # one deleted by hand, or a -NoWait run that has since finished) is a
            # success, not a failure to retry.
            if (-not (Get-AzResourceGroup -Name $m.ResourceGroup -ErrorAction SilentlyContinue)) {
                Write-Host "   resource group $($m.ResourceGroup) no longer exists"
                if ($m.PwdFile -and (Test-Path $m.PwdFile)) { Remove-Item $m.PwdFile -Force -ErrorAction SilentlyContinue }
                Write-Host "   ✅ $($m.Name) removed"
                continue
            }
            Write-Host "   deleting resource group $($m.ResourceGroup)..."
            $job = Remove-AzResourceGroup -Name $m.ResourceGroup -Force -AsJob -ErrorAction Stop
            $pending.Add([pscustomobject]@{ Machine = $m; Job = $job })
        }
        catch {
            Write-Warning "   failed to start deleting $($m.ResourceGroup): $($_.Exception.Message)"
            $remaining.Add($m)
        }
    }

    if ($NoWait -and $pending.Count -gt 0) {
        # Azure carries on deleting server-side after this process exits. The
        # manifest is deliberately left alone: nothing has been *confirmed*
        # gone, and a later -Destroy reconciles it (the groups will by then read
        # as "no longer exists" and drop out).
        Write-Host ""
        Write-Host "● -NoWait: $($pending.Count) resource group deletion(s) are running in Azure; not waiting for them."
        Write-Host "   They complete on their own. Re-run -Destroy later to confirm and clear the manifest,"
        Write-Host "   or watch them with: Get-AzResourceGroup | Where-Object ResourceGroupName -like '$ResourceGroupPrefix-*'"
        return
    }

    if ($pending.Count -gt 0) {
        Write-Host "● Waiting for $($pending.Count) resource group deletion(s) to finish (they run concurrently)..."
        $spent = 0
        while ($pending | Where-Object { $_.Job.State -eq "Running" }) {
            Start-Sleep -Seconds 10
            $spent += 10
            $left = @($pending | Where-Object { $_.Job.State -eq "Running" }).Count
            Write-Host ("   {0,3}s elapsed, {1} still deleting: {2}" -f $spent, $left,
                (@($pending | Where-Object { $_.Job.State -eq "Running" } | ForEach-Object { $_.Machine.Name }) -join ", "))
        }
        foreach ($p in $pending) {
            $m = $p.Machine
            try {
                # Receive-Job rethrows whatever the delete failed with; a job
                # that ended in any state other than Completed is a failure.
                Receive-Job -Job $p.Job -ErrorAction Stop | Out-Null
                if ($p.Job.State -ne "Completed") { throw "deletion job ended in state '$($p.Job.State)'" }
                if ($m.PwdFile -and (Test-Path $m.PwdFile)) { Remove-Item $m.PwdFile -Force -ErrorAction SilentlyContinue }
                Write-Host "   ✅ $($m.Name) removed"
            }
            catch {
                Write-Warning "   failed to delete $($m.ResourceGroup): $($_.Exception.Message)"
                $remaining.Add($m)
            }
            finally {
                Remove-Job -Job $p.Job -Force -ErrorAction SilentlyContinue
            }
        }
    }

    if ($remaining.Count -gt 0) {
        Write-Manifest -Path $ManifestFile -Machines $remaining
        # Write-Host, not Write-Error: see the summary at the end of the script -
        # under $ErrorActionPreference = "Stop" an error record here renders as a
        # crash and swallows the `exit`.
        Write-Host "❌ $($remaining.Count) machine(s) could not be removed and are still in the manifest." -ForegroundColor Red
        exit 1
    }
    Remove-Item $ManifestFile -Force -ErrorAction SilentlyContinue
    Write-Host "✅ Estate torn down."
    return
}

# --- Validation --------------------------------------------------------------
if ($Parallel -and $PSVersionTable.PSVersion.Major -lt 7) {
    throw "-Parallel needs PowerShell 7+ (ForEach-Object -Parallel); you are on $($PSVersionTable.PSVersion). Re-run with pwsh, or drop -Parallel."
}
$total = $Windows + $Ubuntu + $Rocky
if ($total -lt 1) { throw "Nothing to do: -Windows, -Ubuntu and -Rocky are all 0." }
# The Linux setup scripts verify - as the service user - that the enrollment
# material is readable, which the packages only arrange from 0.16.2 on (the
# agent's adopt_owner). Against an older release every Ubuntu/Rocky machine
# fails that check by design, ten minutes of Azure provisioning later; say so
# now instead. Windows is unaffected (the service runs as LocalSystem).
$minLinuxRelease = [version]"0.16.2"
$parsedVersion = $null
if (($Ubuntu -gt 0 -and -not $UbuntuPackageUrl) -or ($Rocky -gt 0 -and -not $RockyPackageUrl)) {
    if ([version]::TryParse($Version, [ref]$parsedVersion) -and $parsedVersion -lt $minLinuxRelease) {
        throw "Release $Version predates the Linux fleet-enrollment fix ($minLinuxRelease): every Ubuntu/Rocky machine would fail its post-enrollment check. Use -Version $minLinuxRelease or newer, or point -UbuntuPackageUrl/-RockyPackageUrl at a build that includes it."
    }
}
if (-not $DryRun) {
    if (-not $FleetServer) { throw "-FleetServer is required (or set `$env:NSCLIENT_FLEET_SERVER)." }
    if (-not $ApiKey) { throw "-ApiKey is required (or set `$env:NSCLIENT_FLEET_API_KEY). Mint one from *API keys* in the fleet UI." }
    if ($FleetServer -match '^http://' -and -not ($FleetInsecure -or $FleetNoVerify)) {
        throw "A plain http:// fleet server sends the bootstrap token in cleartext; pass -FleetInsecure to accept that (test networks only)."
    }
    if ($FleetCaFile -and -not (Test-Path $FleetCaFile)) { throw "-FleetCaFile '$FleetCaFile' does not exist." }
}

# --- Plan --------------------------------------------------------------------
# Names are chosen up front so the plan is printable and collision-free, but the
# tokens are minted per machine inside the worker: they are single-use and
# short-lived, and a sequential six-machine run can easily outlast one.
$manifest = Read-Manifest -Path $ManifestFile
$taken = @($manifest | ForEach-Object { $_.Name })
$machines = [System.Collections.Generic.List[object]]::new()

function Add-FleetMachine {
    param($Os, $Script, $Image, [string[]]$ExtraArgs)
    $name = New-FleetMachineName -Exclude ($taken + @($machines | ForEach-Object { $_.Name }))
    $rg = "$ResourceGroupPrefix-$name"
    $pwdFile = Join-Path $scriptDir ".vm.$name.pwd"
    # Not $args: that is an automatic variable inside a function.
    $setupArgs = @(
        "-VmName", $name,
        "-ResourceGroupName", $rg,
        "-Location", $Location,
        "-AdminUsername", $AdminUsername,
        "-VmSize", $VmSize,
        "-PwdFile", $pwdFile
    ) + $ExtraArgs
    if ($FleetInsecure -or $FleetNoVerify) { $setupArgs += "-FleetInsecure" }
    if ($FleetNoVerify) { $setupArgs += "-FleetNoVerify" }
    if ($FleetCaFile) { $setupArgs += @("-FleetCaFile", (Resolve-Path $FleetCaFile).Path) }

    $machines.Add([pscustomobject]@{
            Name          = $name
            Os            = $Os
            Image         = $Image
            ResourceGroup = $rg
            PwdFile       = $pwdFile
            Script        = $Script
            SetupArgs     = $setupArgs
        })
}

# A family with its own package url and no explicit -Version installs whatever
# that package happens to be, so tell the setup script to skip the version check
# rather than fail it against the default release number.
$versionWasGiven = $PSBoundParameters.ContainsKey("Version")
function Get-VersionArgs {
    param([string]$PackageUrl)
    if (-not $PackageUrl) { return @("-Version", $Version) }
    if ($versionWasGiven) { return @("-Version", $Version, "-PackageUrl", $PackageUrl) }
    return @("-PackageUrl", $PackageUrl, "-SkipVersionCheck")
}

for ($i = 0; $i -lt $Windows; $i++) {
    # Round-robin the images so -Windows 3 across two versions gives a mix.
    $image = $WindowsVersions[$i % $WindowsVersions.Count]
    Add-FleetMachine -Os "windows" -Image $image `
        -Script (Join-Path $scriptDir "win/setup-machine.ps1") `
        -ExtraArgs ((Get-VersionArgs -PackageUrl $WindowsPackageUrl) + @("-WindowsVersion", $image))
}
for ($i = 0; $i -lt $Ubuntu; $i++) {
    Add-FleetMachine -Os "linux" -Image "ubuntu-$UbuntuVersion" `
        -Script (Join-Path $scriptDir "linux/setup-ubuntu-machine.ps1") `
        -ExtraArgs ((Get-VersionArgs -PackageUrl $UbuntuPackageUrl) + @("-UbuntuVersion", $UbuntuVersion))
}
for ($i = 0; $i -lt $Rocky; $i++) {
    Add-FleetMachine -Os "linux" -Image "rocky-$RockyVersion" `
        -Script (Join-Path $scriptDir "linux/setup-rocky-machine.ps1") `
        -ExtraArgs ((Get-VersionArgs -PackageUrl $RockyPackageUrl) + @("-RockyVersion", $RockyVersion))
}

$mode = if ($Parallel) { "parallel (max $MaxParallel at once)" } else { "sequential" }
Write-Host "● Plan: $($machines.Count) machine(s), region $Location, $mode"
$machines | ForEach-Object { Write-Host ("   - {0,-14} {1,-16} RG {2}" -f $_.Name, $_.Image, $_.ResourceGroup) }
if ($DryRun) {
    Write-Host "● -DryRun: nothing was created."
    return
}

Write-Host "● Fleet server: $FleetServer"
$health = Test-FleetServer -FleetServer $FleetServer -SkipCertificateCheck:$skipCert
Write-Host "   /healthz -> $health (the machines must reach this url from Azure too)"

& (Join-Path $scriptDir "connect-to-azure.ps1")

# --- Capacity preflight ------------------------------------------------------
# Once the region's vCPU quota is used up Azure refuses the VM create - but only
# *after* the resource group, vnet, NSG, public IP and NIC are already there, so
# the machine fails ten minutes in with a bare "setup exited 1" and leaves
# billable resources behind. A default subscription gets 10 vCPUs per region, so
# what the estate can be is (10 / vCPUs per machine) - ten of the 1-vCPU default,
# five of a 2-vCPU size. Both quotas that apply are checked: the size's own
# family and the regional total. Machines that are already up count against
# both, so an estate you have not torn down shrinks what this run can create.
#
# A failure to *perform* the check (permissions, an API hiccup) is only a
# warning - it must not block a run that would have worked. A definite answer
# (the size is unusable, or the quota is genuinely short) stops the run.
$shortfalls = @()
$sizeProblem = $null
try {
    $sku = Get-AzComputeResourceSku -Location $Location -ErrorAction Stop |
    Where-Object { $_.ResourceType -eq "virtualMachines" -and $_.Name -eq $VmSize } |
    Select-Object -First 1
    if (-not $sku) {
        $sizeProblem = "$VmSize is not offered in $Location. Pick another -VmSize or -Location."
    }
    else {
        # A size can exist in the region and still be blocked for this
        # subscription. A purely *zonal* restriction does not apply to us: these
        # VMs are created without a zone.
        $hardBlocks = @($sku.Restrictions | Where-Object { $_.Type -ne "Zone" })
        if ($hardBlocks) {
            $sizeProblem = "$VmSize is not available to this subscription in ${Location}: $($hardBlocks.ReasonCode -join ', '). Pick another -VmSize or -Location."
        }
    }
    if (-not $sizeProblem) {
        $perMachine = 2
        $vcpus = ($sku.Capabilities | Where-Object { $_.Name -eq "vCPUs" }).Value
        if ($vcpus) { $perMachine = [int]$vcpus }
        $needed = $machines.Count * $perMachine
        # "cores" is the regional total; the family quota is the per-size one.
        $quotaNames = @("cores")
        if ($sku.Family) { $quotaNames += $sku.Family }
        $usage = Get-AzVMUsage -Location $Location -ErrorAction Stop
        foreach ($quotaName in $quotaNames) {
            $u = $usage | Where-Object { $_.Name.Value -eq $quotaName } | Select-Object -First 1
            if (-not $u) { continue }
            $free = $u.Limit - $u.CurrentValue
            if ($free -lt $needed) {
                $shortfalls += "{0}: {1} of {2} vCPUs free, {3} needed" -f $u.Name.LocalizedValue, $free, $u.Limit, $needed
            }
        }
        if ($shortfalls.Count -eq 0) {
            Write-Host "● Capacity: $($machines.Count) x $VmSize ($perMachine vCPU each) = $needed vCPUs, within the $Location quota"
        }
    }
}
catch {
    Write-Warning "Could not check the vCPU quota in $Location ($($_.Exception.Message)); provisioning anyway."
}
if ($sizeProblem) { throw $sizeProblem }
if ($shortfalls.Count -gt 0) {
    throw ("Not enough vCPU quota in $Location for $($machines.Count) machine(s):`n   " +
        ($shortfalls -join "`n   ") +
        "`n   Tear down an estate that is still up (./provision-fleet-machines.ps1 -Destroy -FleetServer $FleetServer)," +
        " provision fewer machines, pick another -Location, or raise the quota in the Azure portal (Subscription > Usage + quotas).")
}

# --- Per-machine worker ------------------------------------------------------
# Mirrors run-all-tests.ps1: the setup script is run as a child process (it calls
# `exit`, which would otherwise take this script down with it), stderr is merged
# into stdout so a sub-script's warnings never become error records that abort a
# parallel fan-out, and every failure is returned as a result rather than thrown.
function Invoke-FleetProvision {
    param($M, $PsExe, $FleetApiScript, $FleetServer, $ApiKey, [bool]$SkipCert)
    $ErrorActionPreference = 'Continue'
    . $FleetApiScript

    function Show { param($line) Write-Host "[$($M.Name)] $line" }

    $result = [pscustomobject]@{
        Name          = $M.Name
        Os            = $M.Os
        Image         = $M.Image
        ResourceGroup = $M.ResourceGroup
        PwdFile       = $M.PwdFile
        HostId        = $null
        PublicIp      = $null
        FleetServer   = $FleetServer
        CreatedAt     = (Get-Date).ToString("s")
        Status        = "FAILED"
        Detail        = $null
    }
    try {
        Show "creating a pending host on the fleet server..."
        $fleetHost = New-FleetHost -FleetServer $FleetServer -ApiKey $ApiKey -SkipCertificateCheck:$SkipCert
        $result.HostId = $fleetHost.HostId
        Show "host $($fleetHost.HostId), token valid until $($fleetHost.ExpiresAt.ToString('HH:mm:ss'))"

        Show "provisioning + installing + enrolling..."
        # The token goes on the child's command line, so it is briefly visible in
        # the local process list. It is single-use and expires within the hour;
        # it also has to travel to the VM inside an Azure RunCommand script
        # either way.
        $childArgs = @($M.SetupArgs) + @("-FleetServer", $FleetServer, "-FleetToken", $fleetHost.BootstrapToken)
        & $PsExe -NoProfile -File $M.Script @childArgs 2>&1 | ForEach-Object { Show $_ }
        if ($LASTEXITCODE -ne 0) { throw "setup exited $LASTEXITCODE" }

        if (Test-Path $M.PwdFile) {
            $ipLine = @(Get-Content $M.PwdFile) -match '^Public IP:'
            if ($ipLine) { $result.PublicIp = ($ipLine[0] -split ':', 2)[1].Trim() }
        }
        $result.Status = "OK"
        Show "OK ($($result.PublicIp))"
    }
    catch {
        $result.Detail = "$($_.Exception.Message)"
        Show "FAILED: $($result.Detail)"
    }
    $result
}

# --- Run ---------------------------------------------------------------------
$psExe = (Get-Process -Id $PID).Path
$results = [System.Collections.Generic.List[object]]::new()

if ($Parallel) {
    $funcDef = ${function:Invoke-FleetProvision}.ToString()
    try {
        $machines | ForEach-Object -ThrottleLimit $MaxParallel -Parallel {
            $ErrorActionPreference = 'Continue'
            ${function:Invoke-FleetProvision} = $using:funcDef
            $machine = $_
            try {
                Invoke-FleetProvision -M $machine -PsExe $using:psExe -FleetApiScript $using:fleetApi `
                    -FleetServer $using:FleetServer -ApiKey $using:ApiKey -SkipCert $using:skipCert
            }
            catch {
                [pscustomobject]@{
                    Name = $machine.Name; Os = $machine.Os; Image = $machine.Image
                    ResourceGroup = $machine.ResourceGroup; PwdFile = $machine.PwdFile
                    HostId = $null; PublicIp = $null; FleetServer = $using:FleetServer
                    CreatedAt = (Get-Date).ToString("s"); Status = "ERROR"; Detail = "$($_.Exception.Message)"
                }
            }
        } | ForEach-Object { $results.Add($_) }
    }
    catch {
        Write-Warning "Parallel run raised an unexpected error: $($_.Exception.Message)"
    }
}
else {
    foreach ($m in $machines) {
        $results.Add((Invoke-FleetProvision -M $m -PsExe $psExe -FleetApiScript $fleetApi `
                    -FleetServer $FleetServer -ApiKey $ApiKey -SkipCert $skipCert))
    }
}

# --- Manifest + summary ------------------------------------------------------
# Record every machine that got as far as an Azure resource group, failures
# included: a half-created VM still costs money and still needs -Destroy.
$manifest = @($manifest) + @($results | Select-Object Name, Os, Image, ResourceGroup, PwdFile, HostId, PublicIp, FleetServer, CreatedAt, Status)
Write-Manifest -Path $ManifestFile -Machines $manifest

Write-Host "`n===== MACHINES ====="
$results | Format-Table -AutoSize Name, Os, Image, PublicIp, HostId, Status, Detail | Out-String | Write-Host
Write-Host "Manifest:  $ManifestFile"
Write-Host "Fleet UI:  $($FleetServer.TrimEnd('/'))  (the machines report in within a sync interval)"
Write-Host "Connect:   ./connect-machine.ps1 -PwdFile .vm.<name>.pwd"
Write-Host "Tear down: ./provision-fleet-machines.ps1 -Destroy -RemoveFleetHosts -FleetServer $FleetServer"

$failed = @($results | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    # Not Write-Error: this script runs with $ErrorActionPreference = "Stop", so
    # an error record here is a *terminating* one - PowerShell renders the
    # summary as an exception block (source line, caret, truncated message) that
    # reads like the script itself crashed, and the `exit` below never runs. The
    # run finished and reported; say so plainly and set the exit code.
    Write-Host ""
    Write-Host "❌ $($failed.Count)/$($results.Count) machine(s) failed:" -ForegroundColor Red
    foreach ($f in $failed) {
        # The table above truncates Detail; print it in full here.
        Write-Host ("   {0,-14} {1,-16} {2}" -f $f.Name, $f.Image, $f.Detail) -ForegroundColor Red
    }
    Write-Host "   Logs: ./linux/show-log.ps1 (or ./win/show-log.ps1) -VmName <name>"
    Write-Host "   Failures are in the manifest too, so -Destroy cleans up whatever was created."
    exit 1
}
Write-Host "✅ All $($results.Count) machine(s) provisioned and enrolled."
