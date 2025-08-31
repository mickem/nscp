#Requires -Module Az.Accounts
#Requires -Module Az.Compute
#Requires -Module Az.Network

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
    [string]$Version = "0.9.14",
    [string]$Arch = "x64",
    [string]$WindowsVersion = "windows-11",
    [string]$AdminUsername = "azureadmin"
)

Write-Host "ℹ️ Connecting to Azure account..."
Connect-AzAccount
Set-AzContext -Subscription (Get-AzSubscription)[0]
Write-Host "✅ Successfully connected to Azure."

$credential = Get-Credential -UserName $AdminUsername -Message "Enter the password for the VM's admin account"

Write-Host "ℹ️ Creating resource group: $ResourceGroupName..."
New-AzResourceGroup -Name $ResourceGroupName -Location $Location

Write-Host "ℹ️ Configuring virtual network and security rules..."
$subnetConfig = New-AzVirtualNetworkSubnetConfig -Name "default-subnet" -AddressPrefix "10.0.0.0/24"
$vnet = New-AzVirtualNetwork -Name "$($VmName)-vnet" -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $subnetConfig
$publicIp = New-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName -Location $Location -AllocationMethod Static -Sku Standard

$nsgRuleRDP = New-AzNetworkSecurityRuleConfig -Name "Allow-RDP" -Protocol Tcp -Direction Inbound -Priority 1000 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "3389" -Access Allow
$nsgRuleWinRM = New-AzNetworkSecurityRuleConfig -Name "Allow-WinRM" -Protocol Tcp -Direction Inbound -Priority 1010 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "5985" -Access Allow
$nsg = New-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName -Location $Location -SecurityRules $nsgRuleRDP, $nsgRuleWinRM

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

Write-Host "ℹ️ Creating the Virtual Machine: $VmName with $Skus..."
$vmConfig = New-AzVMConfig -VMName $VmName -VMSize $VMSize | `
    Set-AzVMOperatingSystem -Windows -ComputerName $VmName -Credential $credential | `
    Set-AzVMSourceImage -PublisherName $PublisherName -Offer $Offer -Skus $Skus -Version $VmVersion | `
    Add-AzVMNetworkInterface -Id $nic.Id

New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $vmConfig

Write-Host "ℹ️ Installing NSCP on VM '$VmName' in resource group '$ResourceGroupName'..."

$MsiUrl = "https://github.com/mickem/nscp/releases/download/${Version}/NSCP-${Version}-${Arch}.msi"
Write-Host "ℹ️ Fetching MSI from URL: $MsiUrl"
$scriptBlock = @"
New-Item -ItemType Directory -Path 'C:\temp' -Force
Invoke-WebRequest -Uri '$($MsiUrl)' -OutFile 'C:\temp\installer.msi'
Start-Process msiexec.exe -ArgumentList '/i C:\temp\installer.msi /qn' -Wait
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

Write-Host "ℹ️️ Checking version $Version on VM '$VmName' in resource group '$ResourceGroupName'..."
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


Write-Host "✅ Script finished! VM '$VmName' is deployed and the MSI installation has been initiated."
