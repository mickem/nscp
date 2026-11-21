<#
.SYNOPSIS
    Publish a fleet bundle that makes every enrolled NSClient++ agent connect
    to an Icinga 2 master as a native Icinga agent.
.DESCRIPTION
    Stage 3b of the agent-mode Icinga flow (see README.md). Composes a bundle
    on the fleet server (POST /api/bundles/compose - the server builds and
    signs the zip) whose configuration enables the IcingaAgent module and the
    check modules and points [/settings/Icinga/agent] at the Icinga VM:
    `parent host`, `parent endpoint` and `parent fingerprint`.

    The bundle carries no checks. In agent mode the master holds them
    (agent-checks.json, installed by setup-icinga-agent-sync.ps1) and sends
    each one down the connection the agent opens.

    It carries no ticket either: a ticket is issued per host name and a bundle
    is the same for every host. The agents ask for a certificate without one
    and the sync timer on the Icinga VM signs the request of every enrolled
    fleet host, usually within a minute.

    The master's certificate comes from its own CA, which the agents do not
    have yet, so the bundle pins its fingerprint and turns trust-on-first-use
    off: the first connection is verified against the pin, later ones against
    the CA the master hands over.

    The bundle is assigned to a group whose selector matches every host in the
    tenant ({"clauses": []}); the group is created if it does not exist.
    Re-running composes a new version (timestamped by default) and moves the
    group's assignment over to it.

    Needs an Owner/Admin API key: composing bundles and managing groups are
    configuration writes.

    IcingaAgent is experimental and not in a release yet: the machines need a
    build that has the module, loaded when the bundle arrives (the setup
    scripts activate it at install time when the package ships it).
.PARAMETER FleetServer
    Fleet server url. Defaults to $env:NSCLIENT_FLEET_SERVER, then to the
    "Fleet Server" line of the .icinga.pwd file.
.PARAMETER ApiKey
    Owner/Admin fleet API key ("nsk_..."). Defaults to
    $env:NSCLIENT_FLEET_API_KEY.
.PARAMETER ParentHost
    The address the agents connect to (port 5665). Defaults to the "Public IP"
    line of the .icinga.pwd file.
.PARAMETER ParentEndpoint
    The master's endpoint name - the common name of its certificate. Defaults
    to the "Icinga Endpoint" line setup-icinga-agent-sync.ps1 added to the
    .icinga.pwd file.
.PARAMETER ParentFingerprint
    SHA-256 fingerprint of the master's certificate. Defaults to the "Icinga
    Fingerprint" line of the .icinga.pwd file.
.PARAMETER PwdFile
    The Icinga credentials file (default: build/powershell/.icinga.pwd). Only
    read when the parameters above are not all given.
.PARAMETER BundleName
    Name of the bundle to compose (default "icinga-agent").
.PARAMETER BundleVersion
    Version for the composed bundle. The default is timestamped
    (1.0.yyyyMMddHHmmss) so every run is unique.
.PARAMETER GroupName
    The group the bundle is assigned to (default "icinga-agents"; created
    with an every-host selector when missing).
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate (self-signed test server).
.PARAMETER DryRun
    Print the config the bundle would carry and stop without touching the
    fleet server.
.EXAMPLE
    $env:NSCLIENT_FLEET_API_KEY = "nsk_..."   # an owner/admin key
    ./add-icinga-agent-bundle.ps1
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [string]$ParentHost = "",
    [string]$ParentEndpoint = "",
    [string]$ParentFingerprint = "",
    [string]$PwdFile = "",
    [string]$BundleName = "icinga-agent",
    [string]$BundleVersion = "",
    [string]$GroupName = "icinga-agents",
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
if (-not $ParentHost -or -not $ParentEndpoint -or -not $ParentFingerprint -or -not $FleetServer) {
    $icinga = Read-IcingaPwdFile -Path $PwdFile
    if (-not $ParentHost) { $ParentHost = $icinga.PublicIp }
    if (-not $ParentEndpoint) { $ParentEndpoint = $icinga.Endpoint }
    if (-not $ParentFingerprint) { $ParentFingerprint = $icinga.Fingerprint }
    if (-not $FleetServer) { $FleetServer = $icinga.FleetServer }
}
if (-not $ParentHost) {
    Write-Error "❌ No Icinga master: pass -ParentHost or run setup-icinga-machine.ps1 first (it writes $PwdFile)."
    exit 1
}
if (-not $ParentEndpoint -or -not $ParentFingerprint) {
    Write-Error "❌ No master endpoint name / fingerprint: run setup-icinga-agent-sync.ps1 first (it adds them to $PwdFile), or pass -ParentEndpoint and -ParentFingerprint."
    exit 1
}
if (-not $FleetServer) {
    Write-Error "❌ No fleet server: pass -FleetServer or set NSCLIENT_FLEET_SERVER."
    exit 1
}

# --- the bundle config -------------------------------------------------------
# Nested JSON whose object path is the INI section path; the agent renders it
# to fleet.ini (see libs/onboarding/sync.cpp). Note the capital I: the section
# is [/settings/Icinga/agent], while IcingaClient's is [/settings/icinga/client].
$configJson = @{
    modules  = @{
        IcingaAgent  = "enabled"
        CheckSystem  = "enabled"
        CheckDisk    = "enabled"
        CheckHelpers = "enabled"
    }
    settings = @{
        Icinga = @{
            agent = @{
                "parent host"        = $ParentHost
                "parent endpoint"    = $ParentEndpoint
                # The master's CA is not on the machines yet. Verify the first
                # connection against this pin and refuse anything else, rather
                # than trust whoever answers at that address.
                "parent fingerprint" = $ParentFingerprint
                "trust on first use" = "false"
            }
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

# The passive flow's bundle makes the same hosts create themselves in Icinga
# under the same names, with services of the same names. Say so now rather
# than leave it to be found as hosts the sync timer keeps skipping.
$passiveGroup = $groups | Where-Object { $_.name -eq "icinga-monitoring" } | Select-Object -First 1
if ($passiveGroup) {
    $passiveAssigned = @(Invoke-FleetApi -FleetServer $FleetServer -Path "/api/groups/$($passiveGroup.id)/bundles" -ApiKey $ApiKey -SkipCertificateCheck:$FleetNoVerify)
    if ($passiveAssigned.Count -gt 0) {
        Write-Warning "Group 'icinga-monitoring' still has the passive bundle assigned. Its hosts create themselves in Icinga through the API under the same names the agent flow registers - unassign it (fleet UI) and install the sync with -ReplacePassive for a clean agent run."
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
Write-Host "Agents pick the bundle up on their next config sync, ask the master for a"
Write-Host "certificate, and are signed by the sync timer within about a minute. Watch"
Write-Host "the checks arrive with:"
Write-Host "  ./build/powershell/icinga/verify-icinga-estate.ps1 -Mode agent"
