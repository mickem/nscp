<#
.SYNOPSIS
    Installs the Azure PowerShell modules required by the NSCP build and deployment scripts.
.DESCRIPTION
    This script installs the Az PowerShell modules used by the setup, install, validate, and
    teardown scripts in the build/powershell directory. It ensures the PowerShell Gallery is
    trusted and installs each required module if it is not already present.
.PARAMETER Scope
    The installation scope for the modules. Defaults to "CurrentUser".
    Use "AllUsers" to install system-wide (requires elevated privileges).
#>
param(
    [ValidateSet("CurrentUser", "AllUsers")]
    [string]$Scope = "CurrentUser"
)

$ErrorActionPreference = "Stop"

# Required Az modules used across the project scripts
$requiredModules = @(
    "Az.Accounts",
    "Az.Compute",
    "Az.Network",
    "Az.Resources"
)

# Ensure the NuGet package provider is available (required for Install-Module)
Write-Host "ℹ️ Ensuring NuGet package provider is available..."
$nuget = Get-PackageProvider -Name NuGet -ErrorAction SilentlyContinue
if (-not $nuget -or $nuget.Version -lt [version]"2.8.5.201") {
    Install-PackageProvider -Name NuGet -MinimumVersion 2.8.5.201 -Force -Scope $Scope | Out-Null
    Write-Host "✅ NuGet package provider installed."
} else {
    Write-Host "✅ NuGet package provider already available."
}

# Trust the PowerShell Gallery so installs don't prompt for confirmation
$gallery = Get-PSRepository -Name PSGallery
if ($gallery.InstallationPolicy -ne "Trusted") {
    Write-Host "ℹ️ Setting PSGallery as a trusted repository..."
    Set-PSRepository -Name PSGallery -InstallationPolicy Trusted
    Write-Host "✅ PSGallery is now trusted."
} else {
    Write-Host "✅ PSGallery is already trusted."
}

# Install each required module
foreach ($moduleName in $requiredModules) {
    $installed = Get-Module -Name $moduleName -ListAvailable -ErrorAction SilentlyContinue
    if ($installed) {
        $latestVersion = ($installed | Sort-Object Version -Descending | Select-Object -First 1).Version
        Write-Host "✅ $moduleName is already installed (version $latestVersion)."
    } else {
        Write-Host "ℹ️ Installing $moduleName..."
        Install-Module -Name $moduleName -Scope $Scope -Repository PSGallery -Force -AllowClobber
        $ver = (Get-Module -Name $moduleName -ListAvailable | Sort-Object Version -Descending | Select-Object -First 1).Version
        Write-Host "✅ $moduleName installed (version $ver)."
    }
}

Write-Host ""
Write-Host "✅ All Azure dependencies are installed. You can now run the NSCP deployment scripts."

