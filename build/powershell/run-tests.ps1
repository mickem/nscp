<#
.SYNOPSIS
    Runs the live/remote acceptance suite (tests/live) against an NSClient++
    instance that has already been provisioned on an Azure VM by the
    setup-*-machine.ps1 scripts.
.DESCRIPTION
    This is the "run tests against the machine" half of the manual flow:

        1. setup-machine.ps1 / setup-ubuntu-machine.ps1 / setup-rocky-machine.ps1
           provision the VM, install NSClient++, run `nscp web install` (WEB on
           8443, self-signed cert, random password) and open the firewall.
        2. run-tests.ps1 (this script) points the Jest live suite at that VM's
           public IP + web password and asserts the REST API and the standard
           checks work.

    The VM's public IP and web password are read, in order of preference, from
    explicit parameters, then a credentials file (.vm.pwd, written by the setup
    scripts), then Azure (Get-AzPublicIpAddress) if the Az modules are present.

    The exact same suite runs locally with no VM at all — see the -Local switch
    and tests/live/README.md. This script only exists to wire the VM's address
    and password into the environment the suite reads (NSCP_TARGET_*).
.PARAMETER ResourceGroupName
    Resource group the VM lives in (used to look up the public IP via Azure).
.PARAMETER VmName
    VM name; its public IP resource is "<VmName>-pip".
.PARAMETER Os
    "windows" or "linux" — passed to the suite as NSCP_TARGET_OS so the
    platform-specific checks run without probing the server. Optional; the
    suite derives it from check_os_version when omitted.
.PARAMETER PublicIp
    Explicit public IP / hostname of the target, overriding the creds file.
.PARAMETER Password
    Explicit WEB password, overriding the creds file.
.PARAMETER User
    REST user to log in as. Defaults to "admin".
.PARAMETER Port
    REST port. Defaults to 8443.
.PARAMETER PwdFile
    Path to the credentials file written by the setup scripts. Defaults to
    "<script dir>/.vm.pwd".
.PARAMETER TestsDir
    Path to the integration-tests directory. Defaults to "<repo>/tests".
.PARAMETER Local
    Skip all VM/Azure lookup and target https://127.0.0.1:<Port>. Use with
    -Password (or NSCP_TARGET_PASSWORD) to test a locally-installed service.
.EXAMPLE
    # After setup-ubuntu-machine.ps1 provisioned NSCP-Ubuntu-Test:
    ./run-tests.ps1 -VmName NSCP-Ubuntu-Test -Os linux
.EXAMPLE
    # Against a local install, no Azure:
    ./run-tests.ps1 -Local -Password 'my-web-pw' -Os linux
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$VmName = "NSCP-Test",
    [ValidateSet("windows", "linux")]
    [string]$Os,
    [string]$PublicIp,
    [string]$Password,
    [string]$User = "admin",
    [int]$Port = 8443,
    [string]$PwdFile,
    [string]$TestsDir,
    [switch]$Local
)

$ErrorActionPreference = "Stop"

# --- Resolve the tests directory (repo-relative by default) -----------------
if (-not $TestsDir) {
    # build/powershell/run-tests.ps1  ->  repo root is two levels up.
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
    $TestsDir = Join-Path $repoRoot "tests"
}
if (-not (Test-Path (Join-Path $TestsDir "package.json"))) {
    throw "Tests directory '$TestsDir' does not look like the integration harness (no package.json). Pass -TestsDir."
}

# --- Resolve target IP + password -------------------------------------------
function Read-VmPwdFile {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $null }
    $ip = $null; $pw = $null
    foreach ($line in Get-Content $Path) {
        if ($line -match '^\s*Public IP:\s*(.+?)\s*$')   { $ip = $Matches[1] }
        if ($line -match '^\s*Web Password:\s*(.+?)\s*$') { $pw = $Matches[1] }
    }
    if ($ip -or $pw) { return @{ Ip = $ip; Password = $pw } }
    return $null
}

if ($Local) {
    if (-not $PublicIp) { $PublicIp = "127.0.0.1" }
} else {
    if (-not $PwdFile) { $PwdFile = Join-Path $PSScriptRoot ".vm.pwd" }

    if (-not $PublicIp -or -not $Password) {
        $fromFile = Read-VmPwdFile -Path $PwdFile
        if ($fromFile) {
            if (-not $PublicIp) { $PublicIp = $fromFile.Ip }
            if (-not $Password) { $Password = $fromFile.Password }
            Write-Host "● Read target from $PwdFile"
        }
    }

    # Last resort: ask Azure for the public IP (password can't be recovered
    # from Azure — it must come from a param or the creds file).
    if (-not $PublicIp) {
        if (Get-Command Get-AzPublicIpAddress -ErrorAction SilentlyContinue) {
            Write-Host "● Looking up public IP for '$VmName-pip' in '$ResourceGroupName'..."
            $PublicIp = (Get-AzPublicIpAddress -Name "$VmName-pip" -ResourceGroupName $ResourceGroupName).IpAddress
        }
    }
}

if (-not $PublicIp)  { throw "Could not determine the target IP. Pass -PublicIp, provide a .vm.pwd file, or run with the Az modules connected." }
if (-not $Password)  { throw "Could not determine the WEB password. Pass -Password or provide a .vm.pwd file written by the setup scripts." }

$targetUrl = "https://$($PublicIp):$Port"
Write-Host "● Target: $targetUrl (user '$User')"

# --- Install harness deps if needed -----------------------------------------
if (-not (Test-Path (Join-Path $TestsDir "node_modules"))) {
    Write-Host "● Installing test harness dependencies (npm install)..."
    Push-Location $TestsDir
    try { npm install } finally { Pop-Location }
}

# --- Run the live suite ------------------------------------------------------
$env:NSCP_TARGET_URL      = $targetUrl
$env:NSCP_TARGET_USER     = $User
$env:NSCP_TARGET_PASSWORD = $Password
$env:NSCP_TARGET_INSECURE = "1"   # VMs use a self-signed cert
if ($Os) { $env:NSCP_TARGET_OS = $Os }

Write-Host "● Running live acceptance suite..."
Push-Location $TestsDir
try {
    npm run test:live
    $exit = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($exit -ne 0) {
    Write-Error "❌ Live acceptance suite failed (exit $exit)."
    exit $exit
}
Write-Host "✅ Live acceptance suite passed against $targetUrl."
