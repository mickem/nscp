#Requires -Module Az.Accounts
#Requires -Module Az.Compute
#Requires -Module Az.Network

<#
.SYNOPSIS
    Used to deploy Ubuntu VM in Azure with NSClient++
.DESCRIPTION
    This is not to be used, it is only for setting up personal test machines
.PARAMETER ResourceGroupName
    The name for the new resource group.
.PARAMETER Location
    The Azure region where resources will be deployed (e.g., "WestEurope", "EastUS").
.PARAMETER VmName
    The name for the new virtual machine.
.PARAMETER Version
    The NSClient++ release to install (e.g. "0.15.0").
.PARAMETER PackageUrl
    Download the DEB from this url instead of the GitHub release for -Version.
.PARAMETER SkipVersionCheck
    Do not assert the installed version against -Version. For -PackageUrl builds
    whose version is whatever the build says it is.
.PARAMETER Arch
    The architecture of the software to install (e.g., "amd64" or "i386").
.PARAMETER UbuntuVersion
    The Ubuntu version to deploy (e.g., "24.04", "22.04", "20.04").
.PARAMETER AdminUsername
    The administrator username for the new VM.
.PARAMETER FleetServer
    Enroll the machine with this NSClient fleet server (e.g.
    "https://fleet.example.com") once the package is installed. Defaults to
    $env:NSCLIENT_FLEET_SERVER. Enrollment needs -FleetToken as well; without
    one, an inherited NSCLIENT_FLEET_SERVER simply means "do not enroll".
.PARAMETER FleetToken
    The one-time bootstrap token for -FleetServer, as minted by POST /api/hosts
    (see fleet-api.ps1) or by the install command in the fleet UI.
.PARAMETER FleetInsecure
    Allow an unauthenticated enrollment - needed for a plain http:// fleet server.
.PARAMETER FleetNoVerify
    Also skip verification of the fleet server certificate (for a self-signed
    test server without -FleetCaFile). Implies -FleetInsecure.
.PARAMETER FleetCaFile
    A local PEM file with the CA that issued the fleet server certificate. It is
    copied to the VM and used to verify the enrollment call.
.PARAMETER VmSize
    Azure VM size. The default is a 1-vCPU size: a subscription's vCPU quota is
    what caps the size of an estate, so halving the vCPUs per machine doubles
    how many fit. Must be a Generation 2 size (every image below is Gen2).
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$Location = "WestEurope",
    [string]$VmName = "NSCP-Ubuntu-Test",
    [string]$Version = "0.11.16",
    [string]$PackageUrl = "",
    [switch]$SkipVersionCheck,
    [string]$Arch = "amd64",
    [string]$UbuntuVersion = "24.04",
    [string]$AdminUsername = "azureadmin",
    # Where to write the credentials file. Defaults to the shared
    # build/powershell/.vm.pwd; the wrapper passes a per-machine path so parallel
    # runs don't clobber each other.
    [string]$PwdFile = "",
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$FleetToken = "",
    [switch]$FleetInsecure,
    [switch]$FleetNoVerify,
    [string]$FleetCaFile = "",
    [string]$VmSize = "Standard_F1as_v7"
)

# Fleet options are all-or-nothing: half of them means a machine that quietly
# never joins the fleet, which is exactly what we are here to test.
if ($FleetServer -and -not $FleetToken) {
    # An explicitly passed -FleetServer without a token is the mistake this
    # check exists for. One inherited from NSCLIENT_FLEET_SERVER is not: that
    # variable is set once and left set, so it must not turn every plain test
    # VM into a failed run - it just means "not enrolling this one".
    if ($PSBoundParameters.ContainsKey("FleetServer")) {
        Write-Error "❌ -FleetServer needs -FleetToken (mint one with fleet-api.ps1's New-FleetHost, or copy it from the fleet UI's install command)."
        exit 1
    }
    Write-Host "● NSCLIENT_FLEET_SERVER is set but no -FleetToken was given: provisioning without fleet enrollment."
    $FleetServer = ""
}
if ($FleetToken -and -not $FleetServer) {
    Write-Error "❌ -FleetToken needs -FleetServer (or set NSCLIENT_FLEET_SERVER)."
    exit 1
}
if ($FleetCaFile -and -not (Test-Path $FleetCaFile)) {
    Write-Error "❌ -FleetCaFile '$FleetCaFile' does not exist."
    exit 1
}

# Shared with the other setup scripts: it also validates an autosaved-but-expired
# context instead of trusting Get-AzContext, which would otherwise let us run on
# to New-AzResourceGroup and fail there.
& (Join-Path (Split-Path -Parent $PSScriptRoot) "connect-to-azure.ps1")

# Run a script on the VM via RunCommand, retrying transient Azure API errors
# (Invoke-AzVMRunCommand can fail with "An error occurred while sending the
# request" even when the VM is healthy; Azure serialises RunCommands per VM).
function Invoke-VMRun {
    param(
        [Parameter(Mandatory)][string]$ResourceGroupName,
        [Parameter(Mandatory)][string]$VMName,
        [Parameter(Mandatory)][string]$ScriptString,
        [int]$Retries = 3
    )
    for ($attempt = 1; $attempt -le $Retries; $attempt++) {
        try {
            return Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName -VMName $VMName `
                -CommandId 'RunShellScript' -ScriptString $ScriptString -ErrorAction Stop
        }
        catch {
            Write-Warning "RunCommand attempt $attempt/$Retries failed: $($_.Exception.Message)"
            if ($attempt -eq $Retries) { throw }
            Start-Sleep -Seconds (15 * $attempt)
        }
    }
}

# Generate SSH key pair for authentication. Use $HOME (set on both Windows
# PowerShell and PowerShell 7 on Linux/macOS) with Join-Path, not the
# Windows-only $env:USERPROFILE with '\' separators — the latter resolves to
# '/.ssh/...' under WSL, so the key is written nowhere and $sshPublicKey ends up
# empty. -N '' is an empty passphrase; '""' would set the literal two-character
# passphrase "" and break `ssh -i` into the VM later.
$sshDir = Join-Path $HOME ".ssh"
if (-not (Test-Path $sshDir)) { New-Item -ItemType Directory -Path $sshDir -Force | Out-Null }
$sshKeyPath = Join-Path $sshDir "az_$($VmName)_rsa"
if (-not (Test-Path $sshKeyPath)) {
    Write-Host "Generating SSH key pair..."
    ssh-keygen -t rsa -b 4096 -f $sshKeyPath -N '' -q
}
$sshPublicKey = Get-Content "$sshKeyPath.pub"

Write-Host "Creating resource group: $ResourceGroupName..."
New-AzResourceGroup -Name $ResourceGroupName -Location $Location -Force

Write-Host "Configuring virtual network and security rules..."
$subnetConfig = New-AzVirtualNetworkSubnetConfig -Name "default-subnet" -AddressPrefix "10.0.0.0/24"
$vnet = New-AzVirtualNetwork -Name "$($VmName)-vnet" -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $subnetConfig
$publicIp = New-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName -Location $Location -AllocationMethod Static -Sku Standard

$nsgRuleSSH = New-AzNetworkSecurityRuleConfig -Name "Allow-SSH" -Protocol Tcp -Direction Inbound -Priority 1000 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "22" -Access Allow
$nsgRuleHTTPS = New-AzNetworkSecurityRuleConfig -Name "Allow-HTTPS" -Protocol Tcp -Direction Inbound -Priority 1010 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "8443" -Access Allow
$nsgRuleNRPE = New-AzNetworkSecurityRuleConfig -Name "Allow-NRPE" -Protocol Tcp -Direction Inbound -Priority 1020 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "5666" -Access Allow
$nsg = New-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName -Location $Location -SecurityRules $nsgRuleSSH, $nsgRuleHTTPS, $nsgRuleNRPE

$nic = New-AzNetworkInterface -Name "$($VmName)-nic" -ResourceGroupName $ResourceGroupName -Location $Location -SubnetId $vnet.Subnets[0].Id -PublicIpAddressId $publicIp.Id -NetworkSecurityGroupId $nsg.Id

# Set Ubuntu image based on version
$PublisherName = "Canonical"

switch ($UbuntuVersion) {
    "24.04" {
        $Offer = "ubuntu-24_04-lts"
        $Skus = "server"
    }
    "22.04" {
        $Offer = "0001-com-ubuntu-server-jammy"
        $Skus = "22_04-lts-gen2"
    }
    "20.04" {
        $Offer = "0001-com-ubuntu-server-focal"
        $Skus = "20_04-lts-gen2"
    }
    "18.04" {
        $Offer = "UbuntuServer"
        $Skus = "18.04-LTS"
    }
    default {
        Write-Error "❌ Unsupported Ubuntu version: $UbuntuVersion. Supported versions: 24.04, 22.04, 20.04, 18.04"
        exit 1
    }
}

Write-Host "Creating the Virtual Machine: $VmName with Ubuntu $UbuntuVersion..."
$vmConfig = New-AzVMConfig -VMName $VmName -VMSize $VMSize | `
    Set-AzVMOperatingSystem -Linux -ComputerName $VmName -Credential (New-Object PSCredential($AdminUsername, (ConvertTo-SecureString -String "TempPassword123!" -AsPlainText -Force))) -DisablePasswordAuthentication | `
    Set-AzVMSourceImage -PublisherName $PublisherName -Offer $Offer -Skus $Skus -Version "latest" | `
    Add-AzVMNetworkInterface -Id $nic.Id | `
    Add-AzVMSshPublicKey -KeyData $sshPublicKey -Path "/home/$AdminUsername/.ssh/authorized_keys"

New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $vmConfig


$vmPublicIp = (Get-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName).IpAddress
Write-Host "Connect via SSH: ssh -i $sshKeyPath $AdminUsername@$vmPublicIp"

Write-Host "Installing NSCP on VM '$VmName' in resource group '$ResourceGroupName'..."

$DebUrl = if ($PackageUrl) { $PackageUrl } else { "https://github.com/mickem/nscp/releases/download/${Version}/NSCP-${Version}-ubuntu-${UbuntuVersion}-${Arch}.deb" }
Write-Host "Fetching DEB from URL: $DebUrl"

# Generate a random password for the web interface
$WebPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 16 | ForEach-Object { [char]$_ })
Write-Host "Generated web interface password."

$scriptBlock = @"
#!/bin/bash
set -e
mkdir -p /tmp/nscp
cd /tmp/nscp
# -nv (not -q) so a bad URL prints the HTTP error (e.g. 404) instead of failing
# silently and surfacing later as a confusing "nscp: command not found".
wget -nv '$DebUrl' -O nscp.deb
# Install the package and let apt resolve ALL of its dependencies from the
# deb's Depends field (libboost-chrono/context, libtinyxml2, libzip, ...) — no
# hand-maintained list to drift out of date. `dpkg -i` unpacks nscp (failing to
# configure while deps are missing), then `apt-get -f install` pulls those deps
# and finishes configuration. This idiom works on a clean VM AND repairs a
# half-configured state left by an earlier attempt (which plain
# `apt-get install ./nscp.deb` refuses to do — it stops with "unmet
# dependencies, run apt --fix-broken").
sudo apt-get update
sudo dpkg -i nscp.deb || true
sudo apt-get install -y -f
# Configure web interface with password. Use a CIDR allow-list, NOT '*': the
# WEB server can't resolve '*' (every REST call 403s), and unquoted it would
# also glob-expand in the shell.
sudo nscp web install --https --allowed-hosts '0.0.0.0/0,::/0' --password '$WebPassword'
# Enable the standard check modules. One --activate-module call PER module:
# the released binary accepts only a single module per call (multi-module
# support is newer), so combining them would silently drop all but the first.
# Without these, every check returns UNKNOWN.
# The `|| true` is required: the released nscp returns a non-zero exit code from
# `settings --activate-module` even on success, which under `set -e` would abort
# the script before the restart below and leave the WEB server down (ECONNREFUSED).
sudo nscp settings --activate-module CheckHelpers || true
sudo nscp settings --activate-module CheckSystem || true
sudo nscp settings --activate-module CheckDisk || true
# NRDPClient and Scheduler are activated here even though nothing uses them
# yet: a fleet bundle that *enables* a module only writes it into fleet.ini, and
# the delayed reload that follows re-reads settings for the plugins already
# loaded - it does not load a newly enabled one. Without this the Nagios bundle
# applies, the host reports "in sync", and nothing is ever submitted until the
# service happens to restart. Loading them up front costs nothing (neither does
# anything without configuration) and keeps the monitoring flow turn-key.
sudo nscp settings --activate-module NRDPClient || true
sudo nscp settings --activate-module Scheduler || true
# Restart the nsclient service (picks up the WEB config and the enabled modules)
sudo systemctl restart nsclient
"@

$result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock

if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}
$result.Value | ForEach-Object { $_.Message }

# --- Fleet enrollment --------------------------------------------------------
# The Linux packages have no install-time enrollment (that is an MSI feature), so
# the host joins the fleet with `nscp enroll`, which is the same code path the
# installer's custom action runs: generate a key pair, post the CSR with the
# bootstrap token, store the returned material as agent-state.json. The core
# starts syncing on the next service start whenever that file exists.
if ($FleetServer) {
    Write-Host "Enrolling VM '$VmName' with the fleet server $FleetServer..."
    $caWrite = ""
    $enrollArgs = @("--server", "'$FleetServer'", "--token", "'$FleetToken'")
    if ($FleetInsecure -or $FleetNoVerify) { $enrollArgs += "--insecure" }
    if ($FleetNoVerify) { $enrollArgs += @("--verify", "none") }
    if ($FleetCaFile) {
        $enrollArgs += @("--ca", "/tmp/fleet-ca.pem")
        # Quoted heredoc: the PEM goes to the VM verbatim, no shell expansion.
        $caWrite = "cat > /tmp/fleet-ca.pem <<'NSCP_FLEET_CA_EOF'`n" + (Get-Content -Path $FleetCaFile -Raw).TrimEnd() + "`nNSCP_FLEET_CA_EOF"
    }
    $scriptBlock = @"
#!/bin/bash
set -e
$caWrite
sudo nscp enroll $($enrollArgs -join ' ')
# The state file lives in the certificate path, which on Linux is under the
# package's shared dir - find it rather than hard-code a path packaging may move.
state=`$(sudo find /usr/lib/nsclient /var/lib/nsclient /etc/nsclient -name agent-state.json 2>/dev/null | head -1)
if [ -z "`$state" ]; then
    echo "FLEET: enrollment reported success but no agent-state.json was written"
    exit 1
fi
# Verify, do not repair.
#
# Enrollment runs under sudo while the packaged service runs unprivileged, so
# the material it writes has to be handed to the service account or the agent
# enrolls, starts its sync, fails to read its own identity and never appears in
# the fleet. The *agent* does that now (onboarding::adopt_owner, plus the
# package post-install for an upgrade). These scripts used to chown it here,
# which fixed the machine and hid the bug at the same time - so the check below
# would have passed no matter what the package did.
svc_user=`$(systemctl show nsclient -p User --value 2>/dev/null)
[ -n "`$svc_user" ] || svc_user=nsclient
# fleet.ini, applied-state.json and the bundle cache live here, next to the
# manifest's parent (the core's fleet folder).
managed="`$(dirname "`$(dirname "`$state")")/fleet"
# Restart so the fleet sync starts now instead of at the next reboot.
sudo systemctl restart nsclient
# Assert what actually broke, as the service user itself: an unreadable manifest
# or an unwritable managed path is a machine that enrolls and never syncs. The
# agent's own log is no help on an older package - it is written to a directory
# the service user cannot create either, so the failure is invisible on the box.
if ! sudo -u "`$svc_user" test -r "`$state"; then
    echo "FLEET: `$svc_user cannot read `$state"
    ls -l "`$state" 2>/dev/null
    echo "FLEET: the installed package does not hand the enrollment material to the service account."
    echo "FLEET: install a build that includes that fix (-PackageUrl), or chown it by hand to keep this machine."
    exit 1
fi
if ! sudo -u "`$svc_user" test -w "`$managed"; then
    echo "FLEET: `$svc_user cannot write `$managed"
    ls -ld "`$managed" 2>/dev/null
    echo "FLEET: the installed package does not create the fleet folder writable by the service account."
    exit 1
fi
# Best-effort: the sync thread dying is reported only to the journal, and only
# for the process now running (an earlier boot's buffered errors flush at
# restart, so match the current PID or they read as a fresh failure).
sleep 10
pid=`$(systemctl show nsclient -p MainPID --value 2>/dev/null)
if [ -n "`$pid" ] && sudo journalctl -u nsclient --no-pager -n 200 2>/dev/null | grep "nscp\[`$pid\]" | grep -q "Fleet sync thread died"; then
    sudo journalctl -u nsclient --no-pager -n 200 | grep "nscp\[`$pid\]" | grep -i fleet | tail -5
    echo "FLEET: the fleet sync thread died after enrollment"
    exit 1
fi
echo "FLEET: enrolled (`$state, service user `$svc_user)"
"@
    $result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock
    if ($result.Status -ne "Succeeded") {
        Write-Error "❌ Failed to enroll the VM with the fleet server. Status: $($result.Status)"
        exit 1
    }
    $messages = @($result.Value | ForEach-Object { $_.Message })
    $messages | ForEach-Object { $_ }
    # RunCommand reports "Succeeded" for a script that exited non-zero, so the
    # marker - not the status - is what says the host actually joined.
    if (-not ($messages -match 'FLEET: enrolled')) {
        Write-Error "❌ The machine did not enroll with $FleetServer (see the output above)."
        exit 1
    }
    Write-Host "✅ Enrolled with the fleet server."
}

if ($SkipVersionCheck) {
    # -PackageUrl without a -Version to compare against (a local build's version
    # is whatever it is), so just report what got installed.
    Write-Host "Skipping the version check (-SkipVersionCheck); installed version:"
    $result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString "nscp --version"
    $result.Value | ForEach-Object { $_.Message }
}
else {

Write-Host "Checking version $Version on VM '$VmName' in resource group '$ResourceGroupName'..."
$scriptBlock = @"
#!/bin/bash
set -e
output=`$(nscp --version 2>&1 || /sbin/nscp --version 2>&1 || /usr/sbin/nscp --version 2>&1 || echo "Could not find nscp")
echo "NSClient++ version: `$output"
if echo "`$output" | grep -q "$Version"; then
    echo "SUCCESS: Version matches expected version $Version."
else
    echo "FAILURE: Version does not match expected version $Version. Actual output: `$output"
    exit 1
fi
"@

$result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock

if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}

$value0 = $result.Value[0].Message
$value1 = if ($result.Value.Count -gt 1) { $result.Value[1].Message } else { "" }
Write-Host "✅ Version output was: $value0"

if ($value0 -match "SUCCESS: ") {
    Write-Host "✅ Version matches expected version $Version."
} elseif ($value1 -match "SUCCESS: ") {
    Write-Host "✅ Version matches expected version $Version."
} else {
    Write-Error "❌ Version check failed. Output was:"
    Write-Host $value0
    Write-Host $value1
    exit 1
}

Write-Host "✅ Correct version installed!"

}

$vmPublicIp = (Get-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName).IpAddress

# Save credentials to .vm.pwd (same format as the Windows setup script) so
# run-tests.ps1 can point the live acceptance suite at this VM.
if (-not $PwdFile) { $PwdFile = Join-Path (Split-Path $PSScriptRoot -Parent) ".vm.pwd" }
$fleetLine = if ($FleetServer) { "`nFleet Server:   $FleetServer" } else { "" }
@"
VM Name:        $VmName
Resource Group: $ResourceGroupName
Public IP:      $vmPublicIp
SSH:            ssh -i $sshKeyPath $AdminUsername@$vmPublicIp
Admin Username: $AdminUsername
Web URL:        https://$($vmPublicIp):8443
Web Password:   $WebPassword$fleetLine
"@ | Set-Content -Path $pwdFile -Force
Write-Host "● Credentials saved to $pwdFile"

Write-Host "✅ Script finished! VM '$VmName' is deployed and NSCP has been installed."
Write-Host "Connect via SSH: ssh -i $sshKeyPath $AdminUsername@$vmPublicIp"
Write-Host "Web interface: https://$($vmPublicIp):8443"
Write-Host "Web password: $WebPassword"
Write-Host ""
Write-Host "Run the acceptance suite against it with:"
Write-Host "  ./build/powershell/run-tests.ps1 -VmName $VmName -Os linux"
