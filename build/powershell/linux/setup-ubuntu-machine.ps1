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
    The version of the software to install, specified as a URL to the DEB installer.
.PARAMETER Arch
    The architecture of the software to install (e.g., "amd64" or "i386").
.PARAMETER UbuntuVersion
    The Ubuntu version to deploy (e.g., "24.04", "22.04", "20.04").
.PARAMETER AdminUsername
    The administrator username for the new VM.
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$Location = "WestEurope",
    [string]$VmName = "NSCP-Ubuntu-Test",
    [string]$Version = "0.11.16",
    [string]$Arch = "amd64",
    [string]$UbuntuVersion = "24.04",
    [string]$AdminUsername = "azureadmin"
)

Write-Host "Connecting to Azure account..."
if (-not (Get-AzContext)) {
    # WSL / headless hosts have no local browser for the interactive account
    # picker (it hangs on "Please select the account..."), so fall back to
    # device-code auth on Linux/macOS — open the printed URL in any browser
    # and enter the code.
    if ($IsLinux -or $IsMacOS) {
        Connect-AzAccount -UseDeviceAuthentication
    } else {
        Connect-AzAccount
    }
    Set-AzContext -Subscription (Get-AzSubscription)[0]
}
Write-Host "✅ Successfully connected to Azure."

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
$VMSize = "Standard_D2ls_v6"

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

$DebUrl = "https://github.com/mickem/nscp/releases/download/${Version}/NSCP-${Version}-ubuntu-${UbuntuVersion}-${Arch}.deb"
Write-Host "Fetching DEB from URL: $DebUrl"

# Generate a random password for the web interface
$WebPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 16 | ForEach-Object { [char]$_ })
Write-Host "Generated web interface password."

$scriptBlock = @"
#!/bin/bash
set -e
mkdir -p /tmp/nscp
cd /tmp/nscp
wget -q '$DebUrl' -O nscp.deb
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
# Restart the nsclient service (picks up the WEB config and the enabled modules)
sudo systemctl restart nsclient
"@

$result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
    -VMName $VmName `
    -CommandId 'RunShellScript' `
    -ScriptString $scriptBlock

if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}
$result.Value | ForEach-Object { $_.Message }

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

$result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
    -VMName $VmName `
    -CommandId 'RunShellScript' `
    -ScriptString $scriptBlock

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

$vmPublicIp = (Get-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName).IpAddress

# Save credentials to .vm.pwd (same format as the Windows setup script) so
# run-tests.ps1 can point the live acceptance suite at this VM.
$pwdFile = Join-Path (Split-Path $PSScriptRoot -Parent) ".vm.pwd"
@"
VM Name:        $VmName
Resource Group: $ResourceGroupName
Public IP:      $vmPublicIp
SSH:            ssh -i $sshKeyPath $AdminUsername@$vmPublicIp
Admin Username: $AdminUsername
Web URL:        https://$($vmPublicIp):8443
Web Password:   $WebPassword
"@ | Set-Content -Path $pwdFile -Force
Write-Host "● Credentials saved to $pwdFile"

Write-Host "✅ Script finished! VM '$VmName' is deployed and NSCP has been installed."
Write-Host "Connect via SSH: ssh -i $sshKeyPath $AdminUsername@$vmPublicIp"
Write-Host "Web interface: https://$($vmPublicIp):8443"
Write-Host "Web password: $WebPassword"
Write-Host ""
Write-Host "Run the acceptance suite against it with:"
Write-Host "  ./build/powershell/run-tests.ps1 -VmName $VmName -Os linux"
