<#
.SYNOPSIS
    Build an ISO with every Windows installer (MSI) and zip build of one
    NSClient++ release, plus the local cmake-build-* trees, for mounting on a
    test VM (vCenter, Hyper-V, ...).

.DESCRIPTION
    Downloads the release assets from GitHub and lays the ISO out as:

        \NSCP-<version>-x64.msi
        \NSCP-<version>-Win32.msi
        \NSCP-<version>-Win32-legacy-xp.msi
        \...
        \zip-NSCP-<version>-x64\           (the unpacked x64 zip)
        \zip-NSCP-<version>-Win32\
        \...
        \local-<platform>\                (one per local build folder)

    MSIs keep their release names, which already carry the build. Each zip is
    unpacked into a folder named after the zip (prefixed "zip-" so it sorts
    away from the MSIs); a zip whose content sits under a single top-level
    folder is flattened into it. Symbols, docs, web-UI and SBOM assets are
    skipped.

    Local build folders are the cmake-build-<type>-<platform> directories in
    -BuildRoot. Each one that has an nscp.exe is copied to local-<platform>
    in the same layout as a release zip: the runtime files at the root,
    modules\*.dll/*.zip, and the scripts, web, security, licenses and usr
    trees, leaving out CMake/Visual Studio intermediates, PDBs and unit-test
    binaries. If two folders share a platform, the type is appended
    (local-<platform>-<type>).

    The image is written with the Windows built-in IMAPI2 file system
    (ISO9660 + Joliet + UDF), so nothing has to be installed and the disc
    reads on everything back to XP / Server 2003.

.PARAMETER Version
    The release tag, e.g. 0.25.0. Use "latest" for the newest release.
    Omit it to put only the local builds on the ISO.

.PARAMETER OutFile
    Where to write the ISO. Defaults to .\NSCP-<version>-windows.iso.

.PARAMETER WorkDir
    Download/staging directory. Defaults to a fresh folder under %TEMP%,
    removed afterwards unless -KeepWorkDir is given.

.PARAMETER Repo
    GitHub repository, default mickem/nscp.

.PARAMETER BuildRoot
    Where to look for cmake-build-* folders. Defaults to the repository root
    (the parent of this script's folder).

.PARAMETER NoLocal
    Leave the local build folders out.

.EXAMPLE
    .\tools\make-windows-iso.ps1 -Version 0.25.0

.EXAMPLE
    .\tools\make-windows-iso.ps1            # local builds only

.NOTES
    Set GITHUB_TOKEN (or have `gh auth token` work) to avoid the anonymous
    API rate limit.
#>
[CmdletBinding()]
param(
    [string]$Version,
    [string]$OutFile,
    [string]$WorkDir,
    [string]$Repo = 'mickem/nscp',
    [string]$BuildRoot = (Split-Path -Parent $PSScriptRoot),
    [switch]$NoLocal,
    [switch]$KeepWorkDir
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # Invoke-WebRequest is painfully slow with the progress bar on PS 5.1
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Get-GitHubHeaders {
    $headers = @{ 'User-Agent' = 'nscp-make-windows-iso'; 'Accept' = 'application/vnd.github+json' }
    $token = $env:GITHUB_TOKEN
    if (-not $token -and (Get-Command gh -ErrorAction SilentlyContinue)) {
        try { $token = (& gh auth token 2>$null) } catch { $token = $null }
    }
    if ($token) { $headers['Authorization'] = "Bearer $($token.Trim())" }
    return $headers
}

# A Windows build is an .msi, or a .zip naming a Windows architecture that is
# not a by-product (symbols, docs, web UI).
function Test-WindowsAsset([string]$name) {
    if ($name -match '(?i)(symbols|docs|cdx|sha256|^NSCP-Web-)') { return $false }
    if ($name -match '(?i)\.msi$') { return $true }
    if ($name -match '(?i)\.zip$' -and $name -match '(?i)-(Win32|x64|ARM64|x86)([-.])') { return $true }
    return $false
}

# Files a local build tree leaves next to its output that do not belong in an
# install: CMake/MSBuild bookkeeping, debug databases and unit-test binaries.
$script:BuildJunkDirs = @('CMakeFiles', 'Testing', '_deps', '__pycache__', 'x64', 'Win32', 'ARM64',
    'Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel', '*.dir')
$script:BuildJunkFiles = @('*.vcxproj', '*.vcxproj.filters', '*.vcxproj.user', '*.sln', '*.cmake',
    'CMakeCache.txt', '*.pdb', '*.ilk', '*.lib', '*.exp', '*.obj', '*.tlog', '*.log', '*_test.exe',
    'gtest*.dll', 'gmock*.dll', '*.pyc')

function Test-BuildJunk([IO.FileSystemInfo]$item) {
    $patterns = if ($item.PSIsContainer) { $script:BuildJunkDirs } else { $script:BuildJunkFiles }
    foreach ($p in $patterns) { if ($item.Name -like $p) { return $true } }
    return $false
}

function Copy-BuildTree([string]$from, [string]$to) {
    if (-not (Test-Path -LiteralPath $from)) { return }
    New-Item -ItemType Directory -Force $to | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $from -Force) {
        if (Test-BuildJunk $item) { continue }
        $dest = Join-Path $to $item.Name
        if ($item.PSIsContainer) { Copy-BuildTree $item.FullName $dest }
        else { Copy-Item -LiteralPath $item.FullName -Destination $dest }
    }
}

# Copy a local build folder into the layout of a release zip.
function Copy-LocalBuild([string]$buildDir, [string]$target) {
    New-Item -ItemType Directory -Force $target | Out-Null
    $rootFiles = @('*.exe', '*.dll', '*.pyd', '*.zip', '*.ini', '*.map', 'license.txt', 'THIRD-PARTY-NOTICES.md')
    foreach ($item in Get-ChildItem -LiteralPath $buildDir -File -Force) {
        if (Test-BuildJunk $item) { continue }
        foreach ($p in $rootFiles) {
            if ($item.Name -like $p) { Copy-Item -LiteralPath $item.FullName -Destination $target; break }
        }
    }
    # modules\<Name>\ are the per-module build directories; only the
    # top-level plugins and bundles are installed.
    $modules = Join-Path $buildDir 'modules'
    if (Test-Path -LiteralPath $modules) {
        $modTarget = Join-Path $target 'modules'
        New-Item -ItemType Directory -Force $modTarget | Out-Null
        Get-ChildItem -LiteralPath $modules -File -Force |
            Where-Object { ($_.Name -like '*.dll' -or $_.Name -like '*.zip') -and -not (Test-BuildJunk $_) } |
            ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $modTarget }
    }
    foreach ($sub in @('scripts', 'web', 'security', 'licenses', 'usr')) {
        Copy-BuildTree (Join-Path $buildDir $sub) (Join-Path $target $sub)
    }
}

function Get-LocalBuilds([string]$root) {
    $builds = @()
    foreach ($dir in Get-ChildItem -LiteralPath $root -Directory -Filter 'cmake-build-*' -ErrorAction SilentlyContinue) {
        if ($dir.Name -notmatch '^cmake-build-([^-]+)-(.+)$') { continue }
        if (-not (Test-Path -LiteralPath (Join-Path $dir.FullName 'nscp.exe'))) {
            Write-Warning "Skipping $($dir.Name): no nscp.exe (not built?)"
            continue
        }
        $builds += [pscustomobject]@{ Dir = $dir.FullName; Type = $Matches[1]; Platform = $Matches[2] }
    }
    foreach ($b in $builds) {
        $clash = @($builds | Where-Object { $_.Platform -eq $b.Platform }).Count -gt 1
        $name = if ($clash) { "local-$($b.Platform)-$($b.Type)" } else { "local-$($b.Platform)" }
        $b | Add-Member -NotePropertyName Name -NotePropertyValue $name
    }
    return $builds
}

function Write-IsoImage([string]$sourceDir, [string]$isoPath, [string]$volumeName) {
    if (-not ('NscpIsoWriter' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
public static class NscpIsoWriter {
    public static void Save(object stream, string path) {
        IStream src = (IStream)stream;
        byte[] buffer = new byte[1024 * 1024];
        IntPtr readPtr = Marshal.AllocHGlobal(sizeof(int));
        try {
            using (FileStream dst = File.Create(path)) {
                while (true) {
                    src.Read(buffer, buffer.Length, readPtr);
                    int read = Marshal.ReadInt32(readPtr);
                    if (read <= 0) break;
                    dst.Write(buffer, 0, read);
                }
            }
        } finally {
            Marshal.FreeHGlobal(readPtr);
        }
    }
}
'@
    }
    $fs = New-Object -ComObject IMAPI2FS.MsftFileSystemImage
    # FsiFileSystemISO9660 (1) | FsiFileSystemJoliet (2) | FsiFileSystemUDF (4)
    $fs.FileSystemsToCreate = 7
    $fs.VolumeName = $volumeName
    # Media type 13 = DVD+R DL: just sets the capacity ceiling (~8.5 GB).
    $fs.ChooseImageDefaultsForMediaType(13)
    $fs.Root.AddTree($sourceDir, $false)
    $result = $fs.CreateResultImage()
    [NscpIsoWriter]::Save($result.ImageStream, $isoPath)
    [void][Runtime.InteropServices.Marshal]::ReleaseComObject($fs)
}

$headers = Get-GitHubHeaders
$assets = @()
if ($Version) {
    $releaseUrl = if ($Version -eq 'latest') {
        "https://api.github.com/repos/$Repo/releases/latest"
    } else {
        "https://api.github.com/repos/$Repo/releases/tags/$Version"
    }
    Write-Host "Looking up release $Version in $Repo..."
    $release = Invoke-RestMethod -Uri $releaseUrl -Headers $headers
    $Version = $release.tag_name

    $assets = @($release.assets | Where-Object { Test-WindowsAsset $_.name })
    if ($assets.Count -eq 0) { throw "Release $Version has no Windows MSI or zip assets." }
}

$localBuilds = @()
if (-not $NoLocal) { $localBuilds = @(Get-LocalBuilds $BuildRoot) }
if ($assets.Count -eq 0 -and $localBuilds.Count -eq 0) {
    throw "Nothing to put on the ISO: give -Version and/or build something in $BuildRoot."
}

$label = if ($Version) { $Version } else { 'local' }
if (-not $OutFile) { $OutFile = Join-Path (Get-Location) "NSCP-$label-windows.iso" }
$OutFile = [IO.Path]::GetFullPath($OutFile)

$ownWorkDir = -not $WorkDir
if ($ownWorkDir) { $WorkDir = Join-Path ([IO.Path]::GetTempPath()) ("nscp-iso-" + [guid]::NewGuid().ToString('N')) }
$downloadDir = Join-Path $WorkDir 'download'
$stageDir = Join-Path $WorkDir 'iso'
foreach ($d in @($downloadDir, $stageDir)) {
    if (Test-Path $d) { Remove-Item -Recurse -Force $d }
    New-Item -ItemType Directory -Force $d | Out-Null
}

try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $dlHeaders = $headers.Clone()
    $dlHeaders['Accept'] = 'application/octet-stream'

    foreach ($asset in $assets) {
        $file = Join-Path $downloadDir $asset.name
        Write-Host ("Downloading {0} ({1:N1} MB)..." -f $asset.name, ($asset.size / 1MB))
        # The asset API URL works for drafts too and honours the token.
        Invoke-WebRequest -Uri $asset.url -Headers $dlHeaders -OutFile $file -UseBasicParsing
        if ((Get-Item $file).Length -ne $asset.size) { throw "Size mismatch downloading $($asset.name)" }

        if ($asset.name -match '(?i)\.msi$') {
            Copy-Item $file (Join-Path $stageDir $asset.name)
            continue
        }

        $base = [IO.Path]::GetFileNameWithoutExtension($asset.name)
        $target = Join-Path $stageDir "zip-$base"
        $unpack = Join-Path $downloadDir "unpack-$base"
        Write-Host "  Unpacking into zip-$base\"
        [IO.Compression.ZipFile]::ExtractToDirectory($file, $unpack)

        # Flatten a single top-level folder so the build name is the folder name.
        $top = @(Get-ChildItem -LiteralPath $unpack -Force)
        $content = if ($top.Count -eq 1 -and $top[0].PSIsContainer) { $top[0].FullName } else { $unpack }
        Move-Item -LiteralPath $content -Destination $target
    }

    foreach ($build in $localBuilds) {
        Write-Host "Copying local build $(Split-Path -Leaf $build.Dir) into $($build.Name)\"
        Copy-LocalBuild $build.Dir (Join-Path $stageDir $build.Name)
    }

    Write-Host ""
    Write-Host "ISO layout:"
    Get-ChildItem $stageDir | ForEach-Object { Write-Host "  $($_.Name)$(if ($_.PSIsContainer) { '\' })" }

    $volume = ("NSCP_$label" -replace '[^A-Za-z0-9_]', '_')
    if ($volume.Length -gt 32) { $volume = $volume.Substring(0, 32) }
    if (Test-Path $OutFile) { Remove-Item -Force $OutFile }
    Write-Host ""
    Write-Host "Writing $OutFile..."
    Write-IsoImage $stageDir $OutFile $volume
    Write-Host ("Done: {0} ({1:N1} MB)" -f $OutFile, ((Get-Item $OutFile).Length / 1MB))
}
finally {
    if ($ownWorkDir -and -not $KeepWorkDir) {
        Remove-Item -Recurse -Force $WorkDir -ErrorAction SilentlyContinue
    } else {
        Write-Host "Work directory kept: $WorkDir"
    }
}
