#Requires -Module Az.Accounts
#Requires -Module Az.Compute
#Requires -Module Az.Network

<#
.SYNOPSIS
    Used to deploy CentOS Stream VM in Azure with NSClient++
.DESCRIPTION
    This is not to be used, it is only for setting up personal test machines
.PARAMETER ResourceGroupName
    The name for the new resource group.
.PARAMETER Location
    The Azure region where resources will be deployed (e.g., "WestEurope", "EastUS").
.PARAMETER VmName
    The name for the new virtual machine.
.PARAMETER Version
    The version of the software to install.
.PARAMETER Arch
    The architecture of the software to install (e.g., "x86_64").
.PARAMETER CentOSVersion
    The CentOS Stream version to deploy (e.g., "9", "8").
.PARAMETER AdminUsername
    The administrator username for the new VM.
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$Location = "WestEurope",
    [string]$VmName = "NSCP-CentOS-Test",
    [string]$Version = "0.11.13",
    [string]$Arch = "x86_64",
    [string]$CentOSVersion = "9",
    [string]$AdminUsername = "azureadmin"
)

Write-Host "ℹ️ Connecting to Azure account..."
Connect-AzAccount
Set-AzContext -Subscription (Get-AzSubscription)[0]
Write-Host "✅ Successfully connected to Azure."


# Generate SSH key pair for authentication
$sshKeyPath = "$env:USERPROFILE\.ssh\az_$($VmName)_rsa"
if (-not (Test-Path $sshKeyPath)) {
    Write-Host "ℹ️ Generating SSH key pair..."
    ssh-keygen -t rsa -b 4096 -f $sshKeyPath -N '""' -q
}
$sshPublicKey = Get-Content "$sshKeyPath.pub"

Write-Host "ℹ️ Creating resource group: $ResourceGroupName..."
New-AzResourceGroup -Name $ResourceGroupName -Location $Location -Force

Write-Host "ℹ️ Configuring virtual network and security rules..."
$subnetConfig = New-AzVirtualNetworkSubnetConfig -Name "default-subnet" -AddressPrefix "10.0.0.0/24"
$vnet = New-AzVirtualNetwork -Name "$($VmName)-vnet" -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $subnetConfig
$publicIp = New-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName -Location $Location -AllocationMethod Static -Sku Standard

$nsgRuleSSH = New-AzNetworkSecurityRuleConfig -Name "Allow-SSH" -Protocol Tcp -Direction Inbound -Priority 1000 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "22" -Access Allow
$nsgRuleHTTPS = New-AzNetworkSecurityRuleConfig -Name "Allow-HTTPS" -Protocol Tcp -Direction Inbound -Priority 1010 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "8443" -Access Allow
$nsgRuleNRPE = New-AzNetworkSecurityRuleConfig -Name "Allow-NRPE" -Protocol Tcp -Direction Inbound -Priority 1020 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "5666" -Access Allow
$nsg = New-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName -Location $Location -SecurityRules $nsgRuleSSH, $nsgRuleHTTPS, $nsgRuleNRPE

$nic = New-AzNetworkInterface -Name "$($VmName)-nic" -ResourceGroupName $ResourceGroupName -Location $Location -SubnetId $vnet.Subnets[0].Id -PublicIpAddressId $publicIp.Id -NetworkSecurityGroupId $nsg.Id

# Set CentOS Stream image based on version
$VMSize = "Standard_D2ls_v6"

switch ($CentOSVersion) {
    "9" {
        $PublisherName = "resf"
        $Offer = "rockylinux-x86_64"
        $Skus = "9-base"
    }
    "8" {
        $PublisherName = "resf"
        $Offer = "rockylinux-x86_64"
        $Skus = "8-base"
    }
    default {
        Write-Error "❌ Unsupported CentOS/Rocky version: $CentOSVersion. Supported versions: 9, 8"
        exit 1
    }
}

# Accept marketplace terms for Rocky Linux
Write-Host "ℹ️ Accepting marketplace terms for Rocky Linux..."
try {
    $terms = Get-AzMarketplaceTerms -Publisher $PublisherName -Product $Offer -Name $Skus -ErrorAction Stop
    if (-not $terms.Accepted) {
        $terms | Set-AzMarketplaceTerms -Accept
        Write-Host "✅ Marketplace terms accepted."
    } else {
        Write-Host "✅ Marketplace terms already accepted."
    }
} catch {
    # Terms don't exist yet, create and accept them
    Write-Host "ℹ️ Terms not found, accepting for the first time..."
    Set-AzMarketplaceTerms -Publisher $PublisherName -Product $Offer -Name $Skus -Accept
    Write-Host "✅ Marketplace terms accepted."
}

Write-Host "ℹ️ Creating the Virtual Machine: $VmName with Rocky Linux $CentOSVersion..."
$vmConfig = New-AzVMConfig -VMName $VmName -VMSize $VMSize | `
    Set-AzVMOperatingSystem -Linux -ComputerName $VmName -Credential (New-Object PSCredential($AdminUsername, (ConvertTo-SecureString -String "TempPassword123!" -AsPlainText -Force))) -DisablePasswordAuthentication | `
    Set-AzVMSourceImage -PublisherName $PublisherName -Offer $Offer -Skus $Skus -Version "latest" | `
    Add-AzVMNetworkInterface -Id $nic.Id | `
    Add-AzVMSshPublicKey -KeyData $sshPublicKey -Path "/home/$AdminUsername/.ssh/authorized_keys" | `
    Set-AzVMPlan -Publisher $PublisherName -Product $Offer -Name $Skus

New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $vmConfig

Write-Host "ℹ️ Installing NSCP on VM '$VmName' in resource group '$ResourceGroupName'..."

$RpmUrl = "https://github.com/mickem/nscp/releases/download/${Version}/nscp-${Version}-amd64.rpm"
Write-Host "ℹ️ Fetching RPM from URL: $RpmUrl"

# Generate a random password for the web interface
$WebPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 16 | ForEach-Object { [char]$_ })
Write-Host "ℹ️ Generated web interface password."

$scriptBlock = @"
#!/bin/bash
set -e
mkdir -p /tmp/nscp
cd /tmp/nscp
curl -L -o nscp.rpm '$RpmUrl'
# Install EPEL repository for additional dependencies
sudo dnf install -y epel-release || sudo yum install -y epel-release
# Install required dependencies
sudo dnf install -y boost-filesystem boost-program-options boost-thread \
boost-python3 protobuf lua-libs cryptopp || \
sudo yum install -y boost-filesystem boost-program-options boost-thread \
boost-python3 protobuf lua-libs cryptopp
# Install the RPM package
sudo dnf install -y ./nscp.rpm || sudo yum install -y ./nscp.rpm
# Configure web interface with password
sudo nscp web install --https --allowed-hosts '*' --password '$WebPassword'
# Configure firewall
sudo firewall-cmd --permanent --add-port=8443/tcp || true
sudo firewall-cmd --permanent --add-port=5666/tcp || true
sudo firewall-cmd --reload || true
# Restart the nsclient service
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

Write-Host "ℹ️️ Checking version $Version on VM '$VmName' in resource group '$ResourceGroupName'..."
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
Write-Host "✅ Script finished! VM '$VmName' is deployed and NSCP has been installed."
Write-Host "ℹ️ Connect via SSH: ssh -i $sshKeyPath $AdminUsername@$vmPublicIp"
Write-Host "ℹ️ Web interface: https://$($vmPublicIp):8443"
Write-Host "ℹ️ Web password: $WebPassword"
