
<#
.SYNOPSIS
    Used to deploy VM in azure with NSClient++
.DESCRIPTION
    This is not to be used, it is only for setting up personal test machines
.PARAMETER ResourceGroupName
    The name for the new resource group.
.PARAMETER Location
    The Azure region where resources will be deployed (e.g., "WestEurope", "EastUS").
.PARAMETER VmName
    The name for the new virtual machine.
.PARAMETER Version
    The version of the software to install, specified as a URL to the MSI installer.
.PARAMETER Arch
    The architecture of the software to install (e.g., "x64" or "Win32").
.PARAMETER AdminUsername
    The administrator username for the new VM.
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$Location = "WestEurope",
    [string]$VmName = "NSCP-Test",
    [string]$Version = "0.11.17",
    [string]$Arch = "x64",
    [string]$WindowsVersion = "windows-11",
    [string]$AdminUsername = "azureadmin"
)

# Ensure required modules are installed
foreach ($module in @('Az.Accounts', 'Az.Compute', 'Az.Network')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Write-Host "Installing module $module..."
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

Write-Host "● Connecting to Azure account..."
Connect-AzAccount
Set-AzContext -Subscription (Get-AzSubscription)[0]
Write-Host "✅ Successfully connected to Azure."

$credential = Get-Credential -UserName $AdminUsername -Message "Enter the password for the VM's admin account"

Write-Host "● Creating resource group: $ResourceGroupName..."
New-AzResourceGroup -Name $ResourceGroupName -Location $Location -Force

Write-Host "● Configuring virtual network and security rules..."
$subnetConfig = New-AzVirtualNetworkSubnetConfig -Name "default-subnet" -AddressPrefix "10.0.0.0/24"
$vnet = New-AzVirtualNetwork -Name "$($VmName)-vnet" -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $subnetConfig
$publicIp = New-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName -Location $Location -AllocationMethod Static -Sku Standard

$nsgRuleRDP = New-AzNetworkSecurityRuleConfig -Name "Allow-RDP" -Protocol Tcp -Direction Inbound -Priority 1000 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "3389" -Access Allow
$nsgRuleWinRM = New-AzNetworkSecurityRuleConfig -Name "Allow-WinRM" -Protocol Tcp -Direction Inbound -Priority 1010 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "5985" -Access Allow
$nsgRuleHTTPS = New-AzNetworkSecurityRuleConfig -Name "Allow-HTTPS" -Protocol Tcp -Direction Inbound -Priority 1020 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "8443" -Access Allow
$nsgRuleNRPE = New-AzNetworkSecurityRuleConfig -Name "Allow-NRPE" -Protocol Tcp -Direction Inbound -Priority 1030 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "5666" -Access Allow
$nsg = New-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName -Location $Location -SecurityRules $nsgRuleRDP, $nsgRuleWinRM, $nsgRuleHTTPS, $nsgRuleNRPE

$nic = New-AzNetworkInterface -Name "$($VmName)-nic" -ResourceGroupName $ResourceGroupName -Location $Location -SubnetId $vnet.Subnets[0].Id -PublicIpAddressId $publicIp.Id -NetworkSecurityGroupId $nsg.Id


$PublisherName = "MicrosoftWindowsDesktop"
$Offer = "windows-11"
$Skus = "win11-24h2-pro"
$VmVersion = "latest"
$VMSize = "Standard_D2ls_v6"
if ($WindowsVersion -eq "windows-10") {
    $PublisherName = "MicrosoftWindowsServer"
    $Offer = "Windows-10"
    $Skus = "20h2-pro"
    $VmVersion = "latest"
}
if ($WindowsVersion -eq "windows-2019") {
    $PublisherName = "MicrosoftWindowsServer"
    $Offer = "WindowsServer"
    $Skus = "2019-Datacenter"
    $VmVersion = "latest"
}
if ($WindowsVersion -eq "windows-2022") {
    $PublisherName = "MicrosoftWindowsServer"
    $Offer = "WindowsServer"
    $Skus = "2022-Datacenter"
    $VmVersion = "latest"
}
if ($WindowsVersion -eq "windows-2025") {
    $PublisherName = "MicrosoftWindowsServer"
    $Offer = "WindowsServer"
    $Skus = "2025-datacenter-g2"
    $VmVersion = "latest"
    $VMSize = "Standard_D2ls_v6"
}

Write-Host "● Creating the Virtual Machine: $VmName with $Skus..."
$vmConfig = New-AzVMConfig -VMName $VmName -VMSize $VMSize | `
    Set-AzVMOperatingSystem -Windows -ComputerName $VmName -Credential $credential | `
    Set-AzVMSourceImage -PublisherName $PublisherName -Offer $Offer -Skus $Skus -Version $VmVersion | `
    Add-AzVMNetworkInterface -Id $nic.Id

New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $vmConfig

Write-Host "● Installing NSCP on VM '$VmName' in resource group '$ResourceGroupName'..."

$MsiUrl = "https://github.com/mickem/nscp/releases/download/${Version}/NSCP-${Version}-${Arch}.msi"
Write-Host "● Fetching MSI from URL: $MsiUrl"

# Generate a random password for the web interface
$WebPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 16 | ForEach-Object { [char]$_ })
Write-Host "● Generated web interface password."

$scriptBlock = @"
New-Item -ItemType Directory -Path 'C:\temp' -Force
Invoke-WebRequest -Uri '$($MsiUrl)' -OutFile 'C:\temp\installer.msi'
Start-Process msiexec.exe -ArgumentList '/i C:\temp\installer.msi /qn /l*v C:\temp\install.log' -Wait
"@
$result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
    -VMName $VmName `
    -CommandId 'RunPowerShellScript' `
    -ScriptString $scriptBlock
if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}
$Result.Value | ForEach-Object { $_.Message }

Write-Host "● Configuring web server and firewall on VM '$VmName'..."
$scriptBlock = @"
# Configure web interface with password
& 'C:\Program Files\NSClient++\nscp.exe' web install --https --allowed-hosts * --password '$WebPassword'

# Configure Windows Firewall
New-NetFirewallRule -DisplayName 'NSClient++ HTTPS' -Direction Inbound -Protocol TCP -LocalPort 8443 -Action Allow -ErrorAction SilentlyContinue
New-NetFirewallRule -DisplayName 'NSClient++ NRPE' -Direction Inbound -Protocol TCP -LocalPort 5666 -Action Allow -ErrorAction SilentlyContinue

# Restart the NSClient++ service
Restart-Service -Name nscp -ErrorAction SilentlyContinue
"@
$result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
    -VMName $VmName `
    -CommandId 'RunPowerShellScript' `
    -ScriptString $scriptBlock
if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to configure web server on VM. Status: $($result.Status)"
    exit 1
}
$Result.Value | ForEach-Object { $_.Message }

Write-Host "●️ Checking version $Version on VM '$VmName' in resource group '$ResourceGroupName'..."
$scriptBlock = @"
`$output = & 'C:\Program Files\NSClient++\nscp.exe' --version
Write-Output "NSClient++ version: `$output"
if (`$output -like "*$($Version)*") {
    Write-Output "SUCCESS: Version matches expected version $($Version)."
} else {
    throw "FAILURE: Version does not match expected version $($Version). Actual output: `$output"
}
"@
$Result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
    -VMName $VmName `
    -CommandId 'RunPowerShellScript' `
    -ScriptString $scriptBlock

if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}
$value0 = $result.Value[0].Message
$value1 = $result.Value[1].Message
Write-Host "✅ Version output was: $value0."
if ($value0 -match "SUCCESS: ")
{
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
Write-Host "Connect via RDP: $vmPublicIp"
Write-Host "Web interface: https://$($vmPublicIp):8443"
Write-Host "Web password: $WebPassword"
