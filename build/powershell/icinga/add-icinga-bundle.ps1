<#
.SYNOPSIS
    Publish a fleet bundle that makes every enrolled NSClient++ agent submit
    passive check results to an Icinga 2 server through its REST API.
.DESCRIPTION
    Stage 3 of the turn-key Icinga flow (see README.md). Composes a bundle on
    the fleet server (POST /api/bundles/compose - the server builds and signs
    the zip) whose configuration enables the IcingaClient and Scheduler
    modules, points the Icinga target at the Icinga VM, and adds one schedule
    per entry in passive-checks.json on the ICINGA channel. The schedule names
    are the Icinga service names; "host_check" is submitted as the result of
    the host itself.

    The target carries `ensure objects = true`, so nothing has to register the
    machines in Icinga first: each agent creates its own Host and Service
    objects from the "fleet-host" and "fleet-passive-service" templates that
    setup-icinga-machine.ps1 put on the server.

    The bundle is assigned to a group whose selector matches every host in the
    tenant ({"clauses": []}); the group is created if it does not exist. Agents
    pick the bundle up on their next config sync and the first results reach
    Icinga within one check interval.

    Re-running the script composes a new version of the bundle (the fleet
    server rejects a duplicate name+version, so the default version is
    timestamped) and moves the group's assignment over to it.

    Needs an Owner/Admin API key: composing bundles and managing groups are
    configuration writes.

    This bundle and nagios/add-nagios-bundle.ps1 both name their schedules
    after the service (cpu, memory, ...), and a schedule has one channel. Give
    a host one of the two; with both assigned, the higher priority wins the
    schedules they share and the other server sees those services go stale.
.PARAMETER FleetServer
    Fleet server url. Defaults to $env:NSCLIENT_FLEET_SERVER, then to the
    "Fleet Server" line of the .icinga.pwd file.
.PARAMETER ApiKey
    Owner/Admin fleet API key ("nsk_..."). Defaults to
    $env:NSCLIENT_FLEET_API_KEY.
.PARAMETER IcingaUrl
    The Icinga 2 REST API the agents should submit to, e.g.
    "https://1.2.3.4:5665/". Defaults to the "Icinga URL" line of the
    .icinga.pwd file that setup-icinga-machine.ps1 wrote.
.PARAMETER IcingaUser
    The ApiUser the agents authenticate as. Defaults to the "Icinga Agent
    User" line of the .icinga.pwd file.
.PARAMETER IcingaPassword
    That user's password. Defaults to the "Icinga Agent Password" line of the
    .icinga.pwd file.
.PARAMETER VerifyMode
    The target's `verify mode` (default "none"). The Icinga VM's certificate
    comes from its own CA and names the VM, not its address, so the agents
    cannot verify it as provisioned. Pass "peer-cert" together with a `ca` the
    machines already hold when pointing this at a real Icinga server.
.PARAMETER PwdFile
    Where setup-icinga-machine.ps1 saved the Icinga credentials (default:
    build/powershell/.icinga.pwd). Only read when the Icinga parameters are
    not all given.
.PARAMETER BundleName
    Name of the bundle to compose (default "icinga-passive").
.PARAMETER BundleVersion
    Version for the composed bundle. The default is timestamped
    (1.0.yyyyMMddHHmmss) so every run is unique.
.PARAMETER GroupName
    The group the bundle is assigned to (default "icinga-monitoring"; created
    with an every-host selector when missing).
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate (self-signed test server).
.PARAMETER DryRun
    Print the config the bundle would carry and stop without touching the
    fleet server. The password is masked.
.EXAMPLE
    $env:NSCLIENT_FLEET_API_KEY = "nsk_..."   # an owner/admin key
    ./add-icinga-bundle.ps1 -FleetServer https://fleet.example.com
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [string]$IcingaUrl = "",
    [string]$IcingaUser = "",
    [string]$IcingaPassword = "",
    [string]$VerifyMode = "none",
    [string]$PwdFile = "",
    [string]$BundleName = "icinga-passive",
    [string]$BundleVersion = "",
    [string]$GroupName = "icinga-monitoring",
    [switch]$FleetNoVerify,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")
Import-FleetConfig
if (-not $FleetServer) { $FleetServer = $env:NSCLIENT_FLEET_SERVER }
if (-not $ApiKey) { $ApiKey = $env:NSCLIENT_FLEET_API_KEY }

# --- defaults from .icinga.pwd -----------------------------------------------
if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".icinga.pwd" }
if (-not $IcingaUrl -or -not $IcingaUser -or -not $IcingaPassword -or -not $FleetServer) {
    $icinga = Read-IcingaPwdFile -Path $PwdFile
    if (-not $IcingaUrl) { $IcingaUrl = $icinga.IcingaUrl }
    if (-not $IcingaUser) { $IcingaUser = $icinga.AgentUser }
    if (-not $IcingaPassword) { $IcingaPassword = $icinga.AgentPassword }
    if (-not $FleetServer) { $FleetServer = $icinga.FleetServer }
}
if (-not $IcingaUrl -or -not $IcingaUser -or -not $IcingaPassword) {
    Write-Error "❌ No Icinga endpoint: pass -IcingaUrl/-IcingaUser/-IcingaPassword or run setup-icinga-machine.ps1 first (it writes $PwdFile)."
    exit 1
}
if (-not $FleetServer) {
    Write-Error "❌ No fleet server: pass -FleetServer or set NSCLIENT_FLEET_SERVER."
    exit 1
}

# --- the bundle config -------------------------------------------------------
# Nested JSON whose object path is the INI section path; the agent renders it
# to fleet.ini (see libs/onboarding/sync.cpp). The check modules are enabled
# too so the bundle also works for hosts that were not provisioned by our
# scripts - enabling an already-enabled module is a no-op, and a module a
# platform does not have is a logged error, not a failure.
# The interval comes from the catalog only: setup-icinga-machine.ps1 derived
# the templates' freshness window from the same value, and an override here
# would let the two drift apart (services flapping "stale").
$catalog = Read-PassiveCheckCatalog -Path (Join-Path $scriptDir "passive-checks.json")
$interval = $catalog.Interval

$schedules = @{}
foreach ($service in $catalog.Services) {
    $schedules[$service.name] = @{
        command  = $service.command
        interval = $interval
        channel  = "ICINGA"
    }
}
$configJson = @{
    modules  = @{
        IcingaClient = "enabled"
        Scheduler    = "enabled"
        CheckSystem  = "enabled"
        CheckDisk    = "enabled"
        CheckHelpers = "enabled"
    }
    settings = @{
        icinga    = @{
            client = @{
                hostname = "auto"
                targets  = @{
                    default = @{
                        address            = $IcingaUrl
                        username           = $IcingaUser
                        password           = $IcingaPassword
                        "verify mode"      = $VerifyMode
                        # The agent registers itself: a GET per object, and a
                        # PUT from these templates when it is not there yet.
                        "ensure objects"   = "true"
                        "host template"    = "fleet-host"
                        "service template" = "fleet-passive-service"
                        "check command"    = "dummy"
                    }
                }
            }
        }
        scheduler = @{
            schedules = $schedules
        }
    }
}

if ($DryRun) {
    Write-Host "Would compose bundle '$BundleName' on ${FleetServer} and assign it to group '$GroupName' (every host). config.json:"
    $configJson.settings.icinga.client.targets.default.password = "********"
    $configJson | ConvertTo-Json -Depth 16
    return
}

if (-not $ApiKey) {
    Write-Error "❌ No API key: pass -ApiKey or set NSCLIENT_FLEET_API_KEY (needs an owner/admin key)."
    exit 1
}
if (-not $BundleVersion) {
    $BundleVersion = "1.0.$(Get-Date -Format yyyyMMddHHmmss)"
}

Write-Host "Checking the fleet server at $FleetServer..."
Test-FleetServer -FleetServer $FleetServer -SkipCertificateCheck:$FleetNoVerify | Out-Null

# --- ensure the group first --------------------------------------------------
# Composing is the one step that cannot be undone here (the server signs and
# stores a new bundle version, and nothing below deletes bundles), so every
# call that can still fail on rights or connectivity goes before it.
$groups = @(Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups" -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify)
$group = $groups | Where-Object { $_.name -eq $GroupName } | Select-Object -First 1
if ($group) {
    Write-Host "Group '$GroupName' already exists (id $($group.id)); leaving its selector as it is."
}
else {
    Write-Host "Creating group '$GroupName' matching every host..."
    # An empty clause list is the fleet server's documented "match everything".
    $group = Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups" -Method Post -ApiKey $ApiKey `
        -SkipCertificateCheck:$FleetNoVerify -Body @{
        name     = $GroupName
        selector = @{ clauses = @() }
    }
    Write-Host "✅ Created group '$($group.name)' (id $($group.id))."
}

$assigned = @(Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups/$($group.id)/bundles" -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify)

# The Nagios flow's bundle names the same schedules on another channel; say so
# now rather than leave it to be found as services going stale on one side.
$nagiosGroup = $groups | Where-Object { $_.name -eq "nagios-monitoring" } | Select-Object -First 1
if ($nagiosGroup) {
    $nagiosAssigned = @(Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups/$($nagiosGroup.id)/bundles" -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify)
    if ($nagiosAssigned.Count -gt 0) {
        Write-Warning "Group 'nagios-monitoring' still has a bundle assigned. Its schedules carry the same names on the NRDP channel, so each host reports a shared schedule to one server only - unassign it (fleet UI) for a clean Icinga run."
    }
}

# Every run stores a new version and the tenant's bundle count is capped, so
# drop the versions of earlier runs that no group uses any more. Deleting a
# bundle also deletes its assignments, so anything assigned anywhere stays.
$inUse = @(foreach ($g in @(Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups" -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify)) {
        Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups/$($g.id)/bundles" -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify |
            ForEach-Object { "$($_.bundle_id)" }
    })
Remove-FleetBundleVersions -FleetServer $FleetServer -ApiKey $ApiKey -Name $BundleName -Keep $inUse `
    -SkipCertificateCheck:$FleetNoVerify | ForEach-Object { Write-Host "   $_" }

# --- compose the bundle ------------------------------------------------------
Write-Host "Composing bundle $BundleName $BundleVersion..."
$bundle = Invoke-FleetApi -FleetServer $FleetServer -Path "/api/bundles/compose" -Method Post -ApiKey $ApiKey `
    -SkipCertificateCheck:$FleetNoVerify -Body @{
    name        = $BundleName
    version     = $BundleVersion
    config_json = $configJson
}
Write-Host "✅ Composed bundle $($bundle.name) $($bundle.version) (id $($bundle.id))."

# --- move the assignment to the new bundle -----------------------------------
$previous = $assigned | Where-Object { $_.name -eq $BundleName -and $_.bundle_id -ne $bundle.id }
foreach ($old in @($previous)) {
    Write-Host "Unassigning the previous $BundleName bundle ($($old.version))..."
    Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups/$($group.id)/bundles/$($old.bundle_id)" -Method Delete `
        -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify | Out-Null
}
Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups/$($group.id)/bundles" -Method Post -ApiKey $ApiKey `
    -SkipCertificateCheck:$FleetNoVerify -Body @{
    bundle_id = $bundle.id
    priority  = 100
} | Out-Null
Write-Host "✅ Assigned $BundleName $BundleVersion to group '$GroupName'."

Write-Host ""
Write-Host "Agents pick the bundle up on their next config sync; the first passive results"
Write-Host "reach Icinga within one check interval ($interval). Watch them arrive with:"
Write-Host "  ./build/powershell/icinga/verify-icinga-estate.ps1"
