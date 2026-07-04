
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
    [string]$AdminUsername = "azureadmin",
    # Where to write the credentials file. Defaults to the shared
    # build/powershell/.vm.pwd; the wrapper passes a per-machine path so parallel
    # runs don't clobber each other.
    [string]$PwdFile = ""
)

# Ensure required modules are installed
foreach ($module in @('Az.Accounts', 'Az.Compute', 'Az.Network')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Write-Host "Installing module $module..."
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

# Run a script on the VM via RunCommand, retrying transient Azure API errors.
# Invoke-AzVMRunCommand occasionally fails with "An error occurred while sending
# the request" (a client-side HTTP hiccup) even though the VM is healthy —
# especially on the 2nd/3rd call, since Azure serialises RunCommands per VM.
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
                -CommandId 'RunPowerShellScript' -ScriptString $ScriptString -ErrorAction Stop
        }
        catch {
            Write-Warning "RunCommand attempt $attempt/$Retries failed: $($_.Exception.Message)"
            if ($attempt -eq $Retries) { throw }
            Start-Sleep -Seconds (15 * $attempt)
        }
    }
}

# Generate a random password for the VM admin account
$AdminPassword = -join ((65..90) + (97..122) + (48..57) + (33, 35, 37, 38, 42, 64) | Get-Random -Count 20 | ForEach-Object { [char]$_ })
$securePassword = ConvertTo-SecureString $AdminPassword -AsPlainText -Force
$credential = New-Object System.Management.Automation.PSCredential($AdminUsername, $securePassword)
Write-Host "● Generated VM admin password."

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


# $VMSize (Standard_D2ls_v6) is a Generation 2 only size, so every SKU below
# must be Gen2. Gen1 SKUs (e.g. the plain "<year>-Datacenter") fail with
# "cannot boot Hypervisor Generation '1'" — use the "-gensecond" / "-g2" / client
# Gen2 variants instead.
$VmVersion = "latest"
$VMSize = "Standard_D2ls_v6"
switch ($WindowsVersion) {
    "windows-11" {
        $PublisherName = "MicrosoftWindowsDesktop"
        $Offer = "windows-11"
        $Skus = "win11-24h2-pro"
    }
    "windows-10" {
        $PublisherName = "MicrosoftWindowsDesktop"
        $Offer = "Windows-10"
        $Skus = "win10-22h2-pro-g2"
    }
    "windows-2019" {
        $PublisherName = "MicrosoftWindowsServer"
        $Offer = "WindowsServer"
        $Skus = "2019-datacenter-gensecond"
    }
    "windows-2022" {
        $PublisherName = "MicrosoftWindowsServer"
        $Offer = "WindowsServer"
        $Skus = "2022-datacenter-g2"
    }
    "windows-2025" {
        $PublisherName = "MicrosoftWindowsServer"
        $Offer = "WindowsServer"
        $Skus = "2025-datacenter-g2"
    }
    default {
        Write-Error "❌ Unsupported Windows version: $WindowsVersion. Supported: windows-11, windows-10, windows-2019, windows-2022, windows-2025"
        exit 1
    }
}

# Windows client images (10/11) are Trusted Launch images (Gen2 + vTPM + Secure
# Boot); Windows 11 refuses to deploy without it. -SecurityType TrustedLaunch
# defaults vTPM and Secure Boot to enabled. Server SKUs deploy as plain Gen2.
# NOTE: deploying client Windows in Azure needs a subscription with Windows
# client rights (Visual Studio / Enterprise Dev-Test / multi-session AVD); a
# plain pay-as-you-go subscription may reject the image.
$isClient = $WindowsVersion -in @("windows-10", "windows-11")

Write-Host "● Creating the Virtual Machine: $VmName with $Skus..."
if ($isClient) {
    $vmConfig = New-AzVMConfig -VMName $VmName -VMSize $VMSize -SecurityType "TrustedLaunch"
}
else {
    $vmConfig = New-AzVMConfig -VMName $VmName -VMSize $VMSize
}
$vmConfig = $vmConfig | `
    Set-AzVMOperatingSystem -Windows -ComputerName $VmName -Credential $credential | `
    Set-AzVMSourceImage -PublisherName $PublisherName -Offer $Offer -Skus $Skus -Version $VmVersion | `
    Add-AzVMNetworkInterface -Id $nic.Id

New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $vmConfig -ErrorAction Stop

# Save the RDP credentials as soon as the VM exists — the admin password is a
# random value that only lives in this run's memory, and the full .vm.pwd write
# at the very end is skipped if the install/verify below fails. Persisting it now
# means a failed install still leaves you able to RDP in and debug. The final
# write adds the web password once it's configured.
$vmPublicIp = (Get-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName).IpAddress
if (-not $PwdFile) { $PwdFile = Join-Path (Split-Path $PSScriptRoot -Parent) ".vm.pwd" }
@"
VM Name:        $VmName
Resource Group: $ResourceGroupName
Public IP:      $vmPublicIp
RDP:            $vmPublicIp:3389
Admin Username: $AdminUsername
Admin Password: $AdminPassword
"@ | Set-Content -Path $pwdFile -Force
Write-Host "● RDP credentials saved to $pwdFile (web password will be added after install)."

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
$result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock
if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}
$Result.Value | ForEach-Object { $_.Message }

Write-Host "● Configuring web server and firewall on VM '$VmName'..."
$scriptBlock = @"
# Configure web interface with password. Use a CIDR allow-list, NOT '*': the
# WEB server can't resolve '*', so every REST call would 403.
& 'C:\Program Files\NSClient++\nscp.exe' web install --https --allowed-hosts '0.0.0.0/0,::/0' --password '$WebPassword'

# Enable the standard check modules. One --activate-module call PER module:
# the released binary accepts only a single module per call (multi-module
# support is newer), so combining them would silently drop all but the first.
& 'C:\Program Files\NSClient++\nscp.exe' settings --activate-module CheckHelpers
& 'C:\Program Files\NSClient++\nscp.exe' settings --activate-module CheckSystem
& 'C:\Program Files\NSClient++\nscp.exe' settings --activate-module CheckDisk
& 'C:\Program Files\NSClient++\nscp.exe' settings --activate-module CheckEventLog

# Configure Windows Firewall
New-NetFirewallRule -DisplayName 'NSClient++ HTTPS' -Direction Inbound -Protocol TCP -LocalPort 8443 -Action Allow -ErrorAction SilentlyContinue
New-NetFirewallRule -DisplayName 'NSClient++ NRPE' -Direction Inbound -Protocol TCP -LocalPort 5666 -Action Allow -ErrorAction SilentlyContinue

# Restart the NSClient++ service so the WEB config + enabled modules take effect
Restart-Service -Name nscp -ErrorAction SilentlyContinue
Start-Sleep -Seconds 3
# Surface the result so a failed start / missing listener is visible in the run
# output instead of a silent "web UI not accessible" later.
Write-Host '--- nscp service ---'
Get-Service -Name 'nscp*' -ErrorAction SilentlyContinue | Format-Table -AutoSize Name, Status, StartType
Write-Host '--- listeners on 8443 (empty = WEB server did not start) ---'
Get-NetTCPConnection -LocalPort 8443 -State Listen -ErrorAction SilentlyContinue | Format-Table -AutoSize LocalAddress, LocalPort, State
"@
$result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock
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
$Result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock

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

# Save credentials to .vm.pwd file
if (-not $PwdFile) { $PwdFile = Join-Path (Split-Path $PSScriptRoot -Parent) ".vm.pwd" }
@"
VM Name:        $VmName
Resource Group: $ResourceGroupName
Public IP:      $vmPublicIp
RDP:            $vmPublicIp:3389
Admin Username: $AdminUsername
Admin Password: $AdminPassword
Web URL:        https://$($vmPublicIp):8443
Web Password:   $WebPassword
"@ | Set-Content -Path $pwdFile -Force
Write-Host "● Credentials saved to $pwdFile"

Write-Host "✅ Script finished! VM '$VmName' is deployed and NSCP has been installed."
Write-Host "Connect via RDP: $vmPublicIp"
Write-Host "Web interface: https://$($vmPublicIp):8443"
Write-Host "Web password: $WebPassword"
Write-Host ""
Write-Host "Run the acceptance suite against it with:"
Write-Host "  ./build/powershell/run-tests.ps1 -VmName $VmName -Os windows"
