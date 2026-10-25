<#
.SYNOPSIS
    Host the packages of a GitHub Actions build where a test VM can download
    them, and return their urls.
.DESCRIPTION
    Workflow artifacts can only be downloaded with a GitHub token, which a
    test VM does not have. This script downloads the packages of one build
    run with the gh CLI, uploads them to a private blob container in Azure
    and returns a read-only SAS url per package, ready for the -*PackageUrl
    parameters of run-estate.ps1 and provision-fleet-machines.ps1.

    By default the build is the newest successful "Build main" run on main,
    i.e. the last build of main. -RunId picks any other run of a workflow
    that uploads the same artifacts (a pull request's build, for one).

    The packages land in the resource group -ResourceGroup (created on first
    use, with one storage account and a private "msi" container), under
    run-<id>/, and a package already uploaded for that run is not uploaded
    again. Nothing is public: the urls carry a SAS that expires after
    -ExpiryHours. Remove the resource group when you no longer need it:
    Remove-AzResourceGroup -Name NSCP-Packages.

    Needs the gh CLI (authenticated) and an Az session (Connect-AzAccount).
.PARAMETER RunId
    The workflow run to take the packages from. Default: the newest
    successful run of -Workflow on main.
.PARAMETER Families
    Which packages to publish: windows (the x64 MSI), ubuntu (the -UbuntuVersion
    x64 deb), rocky (the -RockyVersion x64 rpm). Default: all three.
.EXAMPLE
    $build = ./estate/publish-build.ps1                 # the last build of main
    ./provision-fleet-machines.ps1 -WindowsPackageUrl $build.WindowsPackageUrl ...
.EXAMPLE
    ./estate/publish-build.ps1 -RunId 40172205677 -Families windows
#>
param(
    [string]$RunId = "",
    [string[]]$Families = @("windows", "ubuntu", "rocky"),
    [string]$Repository = "mickem/nscp",
    [string]$Workflow = "build-main.yml",
    [string]$UbuntuVersion = "24.04",
    [string]$RockyVersion = "9",
    [ValidateRange(1, 168)]
    [int]$ExpiryHours = 12,
    [string]$ResourceGroup = "NSCP-Packages",
    [string]$Location = "westeurope"
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "The gh CLI is needed to download build artifacts (https://cli.github.com/), then run 'gh auth login'."
}
if (-not (Get-Command New-AzStorageBlobSASToken -ErrorAction SilentlyContinue)) {
    throw "The Az.Storage module is missing in this PowerShell ($($PSVersionTable.PSVersion)): Install-Module Az.Storage -Scope CurrentUser."
}
if (-not (Get-AzContext)) { throw "No Az session: run Connect-AzAccount first." }

# "windows,ubuntu" arrives as one string through -File.
$Families = @($Families | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ } | Select-Object -Unique)
foreach ($f in $Families) {
    if (@("windows", "ubuntu", "rocky") -notcontains $f) { throw "Unknown family '$f' (known: windows, ubuntu, rocky)." }
}

# --- Which run ---------------------------------------------------------------
if ($RunId) {
    $run = gh api "repos/$Repository/actions/runs/$RunId" | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw "Could not read run $RunId of $Repository." }
} else {
    # Only a successful run: build-main.yml cancels a run when a newer merge
    # arrives, and a failed one may be missing the packages.
    $runs = gh api "repos/$Repository/actions/workflows/$Workflow/runs?branch=main&status=success&per_page=1" | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw "Could not list the $Workflow runs of $Repository." }
    $run = @($runs.workflow_runs)[0]
    if (-not $run) { throw "$Workflow has no successful run on main." }
    $RunId = "$($run.id)"
}
if ($run.status -ne "completed") { throw "Run $RunId is still $($run.status); its artifacts are not complete." }
$sha = "$($run.head_sha)"
$shortSha = $sha.Substring(0, [Math]::Min(9, $sha.Length))
Write-Host "● Build: run $RunId ($($run.name), $($run.head_branch) $shortSha, $($run.conclusion), $($run.created_at))"
if (-not $PSBoundParameters.ContainsKey("RunId")) {
    # Say so when main has moved on and its newest build has not finished (or failed).
    $head = gh api "repos/$Repository/commits/main" --jq .sha 2>$null
    if ($head -and $head -ne $sha) {
        Write-Warning "main is at $($head.Substring(0, 9)), but its newest successful build is of $shortSha. That is the build being tested."
    }
}

# --- Which artifacts ---------------------------------------------------------
$artifacts = @((gh api "repos/$Repository/actions/runs/$RunId/artifacts?per_page=100" | ConvertFrom-Json).artifacts)
$patterns = @{
    windows = @{ Name = '^NSCP-(?<v>.+)-x64$'; File = '*.msi'; Param = "WindowsPackageUrl" }
    ubuntu  = @{ Name = "^NSCP-(?<v>.+)-ubuntu-$([regex]::Escape($UbuntuVersion))-x64$"; File = '*.deb'; Param = "UbuntuPackageUrl" }
    rocky   = @{ Name = "^NSCP-(?<v>.+)-rocky-$([regex]::Escape($RockyVersion))-x64$"; File = '*.rpm'; Param = "RockyPackageUrl" }
}

# --- Where they go -------------------------------------------------------------
if (-not (Get-AzResourceGroup -Name $ResourceGroup -ErrorAction SilentlyContinue)) {
    New-AzResourceGroup -Name $ResourceGroup -Location $Location -Tag @{ purpose = "nscp-test-packages" } | Out-Null
    Write-Host "  created resource group $ResourceGroup"
}
$account = @(Get-AzStorageAccount -ResourceGroupName $ResourceGroup)[0]
if (-not $account) {
    # Globally unique, 3-24 lowercase letters and digits.
    $name = "nscppkg" + (-join ((97..122) + (48..57) | Get-Random -Count 8 | ForEach-Object { [char]$_ }))
    $account = New-AzStorageAccount -ResourceGroupName $ResourceGroup -Name $name -Location $Location `
        -SkuName Standard_LRS -Kind StorageV2 -AllowBlobPublicAccess $false -MinimumTlsVersion TLS1_2
    Write-Host "  created storage account $name"
}
$ctx = $account.Context
$container = "msi"
if (-not (Get-AzStorageContainer -Name $container -Context $ctx -ErrorAction SilentlyContinue)) {
    New-AzStorageContainer -Name $container -Context $ctx -Permission Off | Out-Null
}

$result = [ordered]@{ RunId = $RunId; HeadSha = $sha; Version = ""; WindowsPackageUrl = ""; UbuntuPackageUrl = ""; RockyPackageUrl = "" }
$work = Join-Path ([IO.Path]::GetTempPath()) "nscp-build-$RunId"
foreach ($family in $Families) {
    $p = $patterns[$family]
    $artifact = $artifacts | Where-Object { $_.name -match $p.Name } | Select-Object -First 1
    if (-not $artifact) { throw "Run $RunId has no $family artifact (looked for $($p.Name); it has: $(@($artifacts.name) -join ', '))." }
    if ($artifact.expired) { throw "Artifact $($artifact.name) of run $RunId has expired." }
    $null = $artifact.name -match $p.Name
    $result.Version = $Matches.v

    $prefix = "run-$RunId/$($artifact.name)/"
    # Indexed, not Select-Object -First: on Windows PowerShell 5.1 stopping
    # Get-AzStorageBlob's pipeline early ends the whole script, silently and
    # with exit code 0.
    $blob = @(Get-AzStorageBlob -Container $container -Prefix $prefix -Context $ctx -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "$prefix$($p.File)" })[0]
    if ($blob) {
        $blobName = $blob.Name
        Write-Host "  $family : $blobName (already uploaded)"
    } else {
        $dir = Join-Path $work $artifact.name
        if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
        # Out-Host: anything gh prints must not join the returned object.
        gh run download $RunId -R $Repository -n $artifact.name -D $dir | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "Could not download $($artifact.name) from run $RunId." }
        $file = Get-ChildItem -LiteralPath $dir -Recurse -File -Filter $p.File | Select-Object -First 1
        if (-not $file) { throw "$($artifact.name) holds no $($p.File)." }
        $blobName = $prefix + $file.Name
        Set-AzStorageBlobContent -File $file.FullName -Container $container -Blob $blobName -Context $ctx -Force | Out-Null
        Write-Host "  $family : uploaded $blobName ($([Math]::Round($file.Length / 1MB, 1)) MB)"
    }
    $result[$p.Param] = New-AzStorageBlobSASToken -Container $container -Blob $blobName -Permission r -Protocol HttpsOnly `
        -ExpiryTime (Get-Date).AddHours($ExpiryHours) -Context $ctx -FullUri
}
if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host "● NSClient++ $($result.Version) from run $RunId, urls valid for $ExpiryHours h"
[pscustomobject]$result
