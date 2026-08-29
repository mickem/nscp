<#
.SYNOPSIS
    Publish a fleet bundle that makes every enrolled NSClient++ agent submit
    passive check results to a Nagios server over NRDP.
.DESCRIPTION
    Stage 3 of the turn-key monitoring flow (see README.md). Composes a bundle
    on the fleet server (POST /api/bundles/compose - the server builds and signs
    the zip) whose configuration enables the NRDPClient and Scheduler modules,
    points the NRDP target at the Nagios VM, and adds one schedule per entry in
    passive-checks.json. The schedule names double as the Nagios service
    descriptions, which is what keeps the two sides of the flow in step.

    The bundle is assigned to a group whose selector matches every host in the
    tenant ({"clauses": []}); the group is created if it does not exist. Agents
    pick the bundle up on their next config sync and the first results reach
    Nagios within one check interval.

    Re-running the script composes a new version of the bundle (the fleet
    server rejects a duplicate name+version, so the default version is
    timestamped) and moves the group's assignment over to it.

    Needs an Owner/Admin API key: composing bundles and managing groups are
    configuration writes.
.PARAMETER FleetServer
    Fleet server url. Defaults to $env:NSCLIENT_FLEET_SERVER, then to the
    "Fleet Server" line of the .nagios.pwd file.
.PARAMETER ApiKey
    Owner/Admin fleet API key ("nsk_..."). Defaults to
    $env:NSCLIENT_FLEET_API_KEY.
.PARAMETER NrdpUrl
    The NRDP endpoint the agents should submit to, e.g. "http://1.2.3.4/nrdp/".
    Defaults to the "NRDP URL" line of the .nagios.pwd file that
    setup-nagios-machine.ps1 wrote.
.PARAMETER NrdpToken
    The NRDP token for -NrdpUrl. Defaults to the "NRDP Token" line of the
    .nagios.pwd file.
.PARAMETER PwdFile
    Where setup-nagios-machine.ps1 saved the Nagios credentials (default:
    build/powershell/.nagios.pwd). Only read when -NrdpUrl/-NrdpToken are not
    given.
.PARAMETER BundleName
    Name of the bundle to compose (default "nagios-nrdp").
.PARAMETER BundleVersion
    Version for the composed bundle. The default is timestamped
    (1.0.yyyyMMddHHmmss) so every run is unique.
.PARAMETER GroupName
    The group the bundle is assigned to (default "nagios-monitoring"; created
    with an every-host selector when missing).
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate (self-signed test server).
.PARAMETER DryRun
    Print the config the bundle would carry and stop without touching the
    fleet server.
.EXAMPLE
    $env:NSCLIENT_FLEET_API_KEY = "nsk_..."   # an owner/admin key
    ./add-nagios-bundle.ps1 -FleetServer https://fleet.example.com
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [string]$NrdpUrl = "",
    [string]$NrdpToken = "",
    [string]$PwdFile = "",
    [string]$BundleName = "nagios-nrdp",
    [string]$BundleVersion = "",
    [string]$GroupName = "nagios-monitoring",
    [switch]$FleetNoVerify,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")

# --- defaults from .nagios.pwd -----------------------------------------------
if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".nagios.pwd" }
if (-not $NrdpUrl -or -not $NrdpToken -or -not $FleetServer) {
    $nagios = Read-NagiosPwdFile -Path $PwdFile
    if (-not $NrdpUrl) { $NrdpUrl = $nagios.NrdpUrl }
    if (-not $NrdpToken) { $NrdpToken = $nagios.NrdpToken }
    if (-not $FleetServer) { $FleetServer = $nagios.FleetServer }
}
if (-not $NrdpUrl -or -not $NrdpToken) {
    Write-Error "❌ No NRDP endpoint: pass -NrdpUrl/-NrdpToken or run setup-nagios-machine.ps1 first (it writes $PwdFile)."
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
# The interval comes from the catalog only: setup-nagios-machine.ps1 derived
# the Nagios freshness threshold from the same value, and an override here
# would let the two drift apart (services flapping "stale").
$catalog = Read-PassiveCheckCatalog -Path (Join-Path $scriptDir "passive-checks.json")
$interval = $catalog.Interval

$schedules = @{}
foreach ($service in $catalog.Services) {
    $schedules[$service.name] = @{
        command  = $service.command
        interval = $interval
        channel  = "NRDP"
    }
}
$configJson = @{
    modules  = @{
        NRDPClient   = "enabled"
        Scheduler    = "enabled"
        CheckSystem  = "enabled"
        CheckDisk    = "enabled"
        CheckHelpers = "enabled"
    }
    settings = @{
        NRDP      = @{
            client = @{
                hostname = "auto"
                targets  = @{
                    default = @{
                        address = $NrdpUrl
                        token   = $NrdpToken
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
Write-Host "reach Nagios within one check interval ($interval). Watch them arrive with:"
Write-Host "  ./build/powershell/nagios/verify-nagios-estate.ps1"
