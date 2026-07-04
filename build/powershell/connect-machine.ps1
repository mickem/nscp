<#
.SYNOPSIS
    Open an interactive session to a provisioned NSClient++ test VM: Remote
    Desktop for Windows, SSH for Linux.
.DESCRIPTION
    Reads the target from the shared credentials file written by the setup
    scripts (build/powershell/.vm.pwd) and picks the right client automatically:

      * a Windows VM (the file has an "RDP:" line + "Admin Password") launches
        mstsc, pre-seeding the password with cmdkey so it doesn't prompt;
      * a Linux VM (the file has an "SSH:" line) runs ssh with the key the setup
        script generated.

    Works from WSL too: it finds mstsc.exe / cmdkey.exe under
    /mnt/c/Windows/System32 when they are not already on PATH.

    Because .vm.pwd only ever describes the LAST machine provisioned, pass
    -PublicIp / -User / -Password / -Os (and -KeyFile for Linux) explicitly to
    reach a different VM, or re-provision the one you want with -KeepVms (see
    run-all-tests.ps1) so its credentials are the current ones.
.PARAMETER Os
    "windows" or "linux". Auto-detected from .vm.pwd when omitted.
.PARAMETER PwdFile
    Credentials file to read. Defaults to "<script dir>/.vm.pwd".
.PARAMETER PublicIp
    Target host, overriding the creds file.
.PARAMETER User
    Login user, overriding the creds file (defaults to the file's Admin Username).
.PARAMETER Password
    Windows admin password for RDP, overriding the creds file.
.PARAMETER KeyFile
    SSH private key for Linux, overriding the one parsed from the file's SSH line.
.PARAMETER PrintOnly
    Show the connection details but do not launch a client.
.EXAMPLE
    # Connect to whatever the last setup run provisioned:
    ./connect-machine.ps1
.EXAMPLE
    # RDP to a specific Windows host:
    ./connect-machine.ps1 -Os windows -PublicIp 20.1.2.3 -User azureadmin -Password 'PW'
#>
param(
    [ValidateSet("windows", "linux", "")]
    [string]$Os = "",
    [string]$PwdFile,
    [string]$PublicIp,
    [string]$User,
    [string]$Password,
    [string]$KeyFile,
    [switch]$PrintOnly
)

$ErrorActionPreference = "Stop"

if (-not $PwdFile) { $PwdFile = Join-Path $PSScriptRoot ".vm.pwd" }

# --- Parse the credentials file (Key: value per line) ------------------------
$fields = @{}
if (Test-Path $PwdFile) {
    foreach ($line in Get-Content $PwdFile) {
        if ($line -match '^\s*([^:]+?):\s*(.+?)\s*$') { $fields[$Matches[1].Trim()] = $Matches[2].Trim() }
    }
}

# Fill anything not given explicitly from the file.
if (-not $Os) {
    if ($fields.ContainsKey("RDP")) { $Os = "windows" }
    elseif ($fields.ContainsKey("SSH")) { $Os = "linux" }
}
if (-not $PublicIp) { $PublicIp = $fields["Public IP"] }
if (-not $User) { $User = $fields["Admin Username"] }
if ($Os -eq "windows" -and -not $Password) { $Password = $fields["Admin Password"] }
if ($Os -eq "linux" -and $fields.ContainsKey("SSH") -and $fields["SSH"] -match '-i\s+(\S+)\s+(\S+)@(\S+)') {
    if (-not $KeyFile) { $KeyFile = $Matches[1] }
    if (-not $User) { $User = $Matches[2] }
    if (-not $PublicIp) { $PublicIp = $Matches[3] }
}

if (-not $Os) { throw "Could not determine OS. Pass -Os, or provide a .vm.pwd with an RDP:/SSH: line." }
if (-not $PublicIp) { throw "Could not determine the target IP. Pass -PublicIp or provide a .vm.pwd file." }

# --- Locate a Windows tool, whether on native Windows or under WSL -----------
function Find-WinExe([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in @("/mnt/c/Windows/System32/$name", "C:\Windows\System32\$name")) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

if ($Os -eq "windows") {
    Write-Host "● Windows target ${PublicIp}:3389  (user '$User')"
    if ($Password) { Write-Host "  Admin password: $Password" }
    Write-Host "  Web UI:         https://${PublicIp}:8443"
    if ($PrintOnly) { return }

    $cmdkey = Find-WinExe "cmdkey.exe"
    if ($cmdkey -and $User -and $Password) {
        & $cmdkey "/generic:TERMSRV/$PublicIp" "/user:$User" "/pass:$Password" | Out-Null
        Write-Host "  Cached RDP credentials (cmdkey) so mstsc won't prompt."
    }
    $mstsc = Find-WinExe "mstsc.exe"
    if ($mstsc) {
        Write-Host "  Launching Remote Desktop..."
        Start-Process $mstsc -ArgumentList "/v:$PublicIp"
    }
    else {
        Write-Warning ("mstsc.exe not found. Connect manually to ${PublicIp}:3389 as '$User', " +
            "or with a Linux RDP client, e.g.:  xfreerdp /v:$PublicIp /u:$User")
    }
}
else {
    if (-not $User) { $User = "azureadmin" }
    Write-Host "● Linux target $PublicIp  (user '$User'$(if ($KeyFile) { ", key $KeyFile" }))"
    Write-Host "  Web UI: https://${PublicIp}:8443"
    if ($PrintOnly) { return }

    $sshArgs = @()
    if ($KeyFile) { $sshArgs += @("-i", $KeyFile) }
    # Throwaway VMs reuse public IPs, so skip host-key checking to avoid the
    # "REMOTE HOST IDENTIFICATION HAS CHANGED" wall and first-connect prompt.
    $sshArgs += @("-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=/dev/null", "$User@$PublicIp")
    Write-Host "  ssh $($sshArgs -join ' ')"
    & ssh @sshArgs
}
