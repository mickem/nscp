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
.PARAMETER MsiUrl
    The direct download URL for the .msi file you want to install.
.PARAMETER AdminUsername
    The administrator username for the new VM.
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$Location = "WestEurope",
    [string]$VmName = "NSCP-TestMachine",
    [string]$MsiUrl = "https://github.com/mickem/nscp/releases/download/0.9.12/NSCP-0.9.12-x64.msi",
    [string]$AdminUsername = "azureadmin"
)

Write-Host "Connecting to Azure account..."
Connect-AzAccount
Set-AzContext -Subscription (Get-AzSubscription)[0]
Write-Host "Successfully connected to Azure."

$credential = Get-Credential -UserName $AdminUsername -Message "Enter the password for the VM's admin account"

Write-Host "Creating resource group: $ResourceGroupName..."
New-AzResourceGroup -Name $ResourceGroupName -Location $Location

Write-Host "Configuring virtual network and security rules..."
$subnetConfig = New-AzVirtualNetworkSubnetConfig -Name "default-subnet" -AddressPrefix "10.0.0.0/24"
$vnet = New-AzVirtualNetwork -Name "$($VmName)-vnet" -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $subnetConfig
$publicIp = New-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName -Location $Location -AllocationMethod Static -Sku Standard

$nsgRuleRDP = New-AzNetworkSecurityRuleConfig -Name "Allow-RDP" -Protocol Tcp -Direction Inbound -Priority 1000 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "3389" -Access Allow
$nsgRuleWinRM = New-AzNetworkSecurityRuleConfig -Name "Allow-WinRM" -Protocol Tcp -Direction Inbound -Priority 1010 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "5985" -Access Allow
$nsg = New-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName -Location $Location -SecurityRules $nsgRuleRDP, $nsgRuleWinRM

$nic = New-AzNetworkInterface -Name "$($VmName)-nic" -ResourceGroupName $ResourceGroupName -Location $Location -SubnetId $vnet.Subnets[0].Id -PublicIpAddressId $publicIp.Id -NetworkSecurityGroupId $nsg.Id

Write-Host "Creating the Virtual Machine: $VmName..."
$vmConfig = New-AzVMConfig -VMName $VmName -VMSize "Standard_D2ls_v6" | `
    Set-AzVMOperatingSystem -Windows -ComputerName $VmName -Credential $credential | `
    Set-AzVMSourceImage -PublisherName "MicrosoftWindowsDesktop" -Offer "windows-11" -Skus "win11-24h2-pro" -Version "latest" | `
    Add-AzVMNetworkInterface -Id $nic.Id

New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $vmConfig

Write-Host "Waiting for VM to be ready, then installing MSI..."
$scriptBlock = @"
New-Item -ItemType Directory -Path 'C:\temp' -Force
Invoke-WebRequest -Uri '$($MsiUrl)' -OutFile 'C:\temp\installer.msi'
Start-Process msiexec.exe -ArgumentList '/i C:\temp\installer.msi /qn' -Wait
"@
Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
    -VMName $VmName `
    -CommandId 'RunPowerShellScript' `
    -ScriptString $scriptBlock

Write-Host "✅ Script finished! VM '$VmName' is deployed and the MSI installation has been initiated."
