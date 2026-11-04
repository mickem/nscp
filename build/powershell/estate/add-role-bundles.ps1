<#
.SYNOPSIS
    Compose and assign one fleet bundle per role in an estate manifest.
.DESCRIPTION
    For every role in the manifest that declares a "bundle" (see README.md,
    "Adding a role"), this ensures a group role-<name> whose selector matches
    the role's tags - the operator-set tags provisioning puts on each host
    before it enrolls - composes a bundle role-<name> from the role's modules
    and bundle settings (${ROLE_PASSWORD} substituted), and moves the group's
    assignment to it. Roles without a bundle (base) are skipped.

    Re-running composes a new version (the default version is timestamped) and
    moves the assignment over, as nagios/add-nagios-bundle.ps1 does. The two
    can be layered on one estate: their groups and settings do not overlap.

    Needs an Owner/Admin API key: composing bundles and managing groups are
    configuration writes.
.PARAMETER ManifestFile
    The estate manifest (default: build/powershell/.fleet-machines.json).
.PARAMETER FleetServer
    Fleet server url. Defaults to the manifest's "FleetServer", then to
    $env:NSCLIENT_FLEET_SERVER.
.PARAMETER ApiKey
    Owner/Admin fleet API key. Defaults to $env:NSCLIENT_FLEET_API_KEY.
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate.
.PARAMETER Role
    Only these roles (default: every role in the manifest with a bundle).
.PARAMETER BundleVersion
    Version for the composed bundles. Default 1.0.<yyyyMMddHHmmss>.
.PARAMETER DryRun
    Print each group selector and bundle config (passwords masked) and stop
    without touching the fleet server.
.EXAMPLE
    ./estate/add-role-bundles.ps1 -ManifestFile .estate-runs/20261003-101500/suite-mysql/manifest.json
#>
param(
    [string]$ManifestFile = "",
    [string]$FleetServer = "",
    [string]$ApiKey = $env:NSCLIENT_FLEET_API_KEY,
    [switch]$FleetNoVerify,
    [string[]]$Role = @(),
    [string]$BundleVersion = "",
    [string]$RolesDir = "",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$parentDir = Split-Path -Parent $PSScriptRoot
. (Join-Path $parentDir "fleet-api.ps1")
Import-FleetConfig
if (-not $ApiKey) { $ApiKey = $env:NSCLIENT_FLEET_API_KEY }
. (Join-Path $PSScriptRoot "estate-api.ps1")

if (-not $ManifestFile) { $ManifestFile = Join-Path $parentDir ".fleet-machines.json" }
$machines = @(Read-FleetManifest -Path $ManifestFile)
if (-not $RolesDir) { $RolesDir = Get-EstateRolesDir }
$catalog = Get-EstateRoleCatalog -RolesDir $RolesDir

# The estate's own groups: two estates of one role (a kept one and a new run)
# must not share a group, a bundle or a password.
$estateId = Get-EstateId -Machines $machines
if (-not $estateId) {
    Write-Warning "$ManifestFile predates estate ids: using the shared role-<role> groups, which every other estate of the same role shares too."
}
$roleNames = @($machines | ForEach-Object { "$($_.Role)" } | Where-Object { $_ } | Sort-Object -Unique)
if ($Role.Count -gt 0) { $roleNames = @($roleNames | Where-Object { $Role -contains $_ }) }
$plans = foreach ($name in $roleNames) {
    if (-not $catalog.Contains($name)) {
        Write-Warning "Role '$name' is in the manifest but not in $RolesDir; skipping it."
        continue
    }
    $r = $catalog[$name]
    if (-not $r.Bundle) { continue }
    $password = Get-EstateRolePassword -RoleName $name -Machines $machines
    $clauses = @($r.Tags.Keys | ForEach-Object { @{ op = "eq"; key = $_; value = $r.Tags[$_] } })
    if ($estateId) { $clauses += @{ op = "eq"; key = "estate"; value = $estateId } }
    [pscustomobject]@{
        Role     = $r
        Name     = Get-EstateGroupName -RoleName $name -EstateId $estateId
        # Implicit AND of one eq clause per tag (the role's, and the estate's,
        # which provisioning put on every host); source defaults to manual,
        # so a host cannot put itself in the group by reporting the tag.
        Selector = @{ clauses = $clauses }
        Config   = ConvertTo-EstateBundleConfig -Role $r -RolePassword $password -Machines $machines
        Password = $password
        # Set when the role ships scripts: the bundle is then built here and
        # uploaded whole rather than composed on the server.
        Scripts  = $r.BundleScriptsDir
    }
}
$plans = @($plans)
if ($plans.Count -eq 0) {
    Write-Host "No role in $ManifestFile has a bundle; nothing to compose."
    return
}

if ($DryRun) {
    foreach ($p in $plans) {
        Write-Host "Group '$($p.Name)', selector:"
        ConvertTo-Json -InputObject $p.Selector -Depth 8 | Write-Host
        Write-Host "Bundle '$($p.Name)' config_json:"
        (ConvertTo-Json -InputObject $p.Config -Depth 16).Replace($p.Password, "<role password>") | Write-Host
        if ($p.Scripts) {
            Write-Host "Bundle '$($p.Name)' scripts (uploaded, from $($p.Scripts)):"
            foreach ($f in @(Get-ChildItem -Path $p.Scripts -File -Recurse | Sort-Object FullName)) {
                Write-Host "   scripts/$($f.FullName.Substring($p.Scripts.Length).TrimStart('\', '/').Replace('\', '/'))"
            }
        }
    }
    return
}

if (-not $FleetServer) { $FleetServer = "$(@($machines | Where-Object { $_.FleetServer } | Select-Object -First 1).FleetServer)" }
if (-not $FleetServer) { $FleetServer = $env:NSCLIENT_FLEET_SERVER }
if (-not $FleetServer) { Write-Error "No fleet server: pass -FleetServer or set NSCLIENT_FLEET_SERVER."; exit 1 }
if (-not $ApiKey) { Write-Error "No API key: pass -ApiKey or set NSCLIENT_FLEET_API_KEY (needs an owner/admin key)."; exit 1 }
if (-not $BundleVersion) { $BundleVersion = "1.0.$(Get-Date -Format yyyyMMddHHmmss)" }
$api = @{ FleetServer = $FleetServer; ApiKey = $ApiKey; SkipCertificateCheck = $FleetNoVerify }

$groups = @(Invoke-FleetApi @api -Path "/api/groups")
foreach ($p in $plans) {
    # Group first, compose last: composing is the one step nothing here undoes,
    # so everything that can still fail on rights or connectivity goes before it.
    $group = $groups | Where-Object { $_.name -eq $p.Name } | Select-Object -First 1
    if ($group) {
        # Compared as sorted "op key=value" strings: JSON text would differ on
        # key order alone.
        $want = (@($p.Selector.clauses | ForEach-Object { "$($_.op) $($_.key)=$($_.value)" }) | Sort-Object) -join ", "
        $have = (@($group.selector.clauses | ForEach-Object { "$($_.op) $($_.key)=$($_.value)$(if ($_.source) { " ($($_.source))" })" }) | Sort-Object) -join ", "
        if ($want -ne $have) {
            Write-Warning "Group '$($p.Name)' selects $have, but the role's tags say $want; leaving it as it is. Delete the group to have it recreated."
        }
    }
    else {
        $group = Invoke-FleetApi @api -Path "/api/groups" -Method Post -Body @{ name = $p.Name; selector = $p.Selector }
        Write-Host "Created group '$($group.name)' (id $($group.id))."
    }
    $assigned = @(Invoke-FleetApi @api -Path "/api/groups/$($group.id)/bundles" |
            Where-Object { $_.name -eq $p.Name } | ForEach-Object { "$($_.bundle_id)" })

    # Every run composes a new version and the tenant's bundle count is capped,
    # so versions left over from earlier runs go first - all but the one still
    # assigned, which hosts are running until the new one replaces it.
    Remove-FleetBundleVersions @api -Name $p.Name -Keep $assigned | ForEach-Object { Write-Host "   $_" }

    if ($p.Scripts) {
        $zip = New-EstateBundleZip -Name $p.Name -Version $BundleVersion -Config $p.Config -ScriptsDir $p.Scripts
        $bundle = Invoke-FleetBundleUpload @api -Name $p.Name -Version $BundleVersion -Bundle $zip
        Write-Host "Uploaded bundle $($bundle.name) $($bundle.version) (id $($bundle.id)) with the scripts under $($p.Scripts)."
    }
    else {
        $bundle = Invoke-FleetApi @api -Path "/api/bundles/compose" -Method Post -Body @{
            name        = $p.Name
            version     = $BundleVersion
            config_json = $p.Config
        }
        Write-Host "Composed bundle $($bundle.name) $($bundle.version) (id $($bundle.id))."
    }
    Invoke-FleetApi @api -Path "/api/groups/$($group.id)/bundles" -Method Post -Body @{ bundle_id = $bundle.id; priority = 100 } | Out-Null
    Write-Host "Assigned $($p.Name) $BundleVersion to group '$($p.Name)'."

    # Deleting the previous version also drops its assignment, so this is the
    # unassign step as well.
    Remove-FleetBundleVersions @api -Name $p.Name -Keep @($bundle.id) | ForEach-Object { Write-Host "   $_" }
}
