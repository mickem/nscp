
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
    The NSClient++ release to install (e.g. "0.15.0").
.PARAMETER PackageUrl
    Download the MSI from this url instead of the GitHub release for -Version.
.PARAMETER SkipVersionCheck
    Do not assert the installed version against -Version. For -PackageUrl builds
    whose version is whatever the build says it is.
.PARAMETER Arch
    The architecture of the software to install (e.g., "x64" or "Win32").
.PARAMETER AdminUsername
    The administrator username for the new VM.
.PARAMETER FleetServer
    Enroll the machine with this NSClient fleet server (e.g.
    "https://fleet.example.com"). Defaults to $env:NSCLIENT_FLEET_SERVER.
    Enrollment needs -FleetToken as well; without one, an inherited
    NSCLIENT_FLEET_SERVER simply means "do not enroll".
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
    [string]$VmName = "NSCP-Test",
    [string]$Version = "0.11.17",
    [string]$PackageUrl = "",
    [switch]$SkipVersionCheck,
    [string]$Arch = "x64",
    [string]$WindowsVersion = "windows-11",
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

# Ensure required modules are installed
foreach ($module in @('Az.Accounts', 'Az.Compute', 'Az.Network')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Write-Host "Installing module $module..."
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

# Shared with the other setup scripts: it also validates an autosaved-but-expired
# context instead of trusting Get-AzContext, which would otherwise let us run on
# to New-AzResourceGroup and fail there.
& (Join-Path (Split-Path -Parent $PSScriptRoot) "connect-to-azure.ps1")

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


# -VmSize (default Standard_F1as_v7) is a Generation 2 only size, so every SKU below
# must be Gen2. Gen1 SKUs (e.g. the plain "<year>-Datacenter") fail with
# "cannot boot Hypervisor Generation '1'" — use the "-gensecond" / "-g2" / client
# Gen2 variants instead.
$VmVersion = "latest"
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

$MsiUrl = if ($PackageUrl) { $PackageUrl } else { "https://github.com/mickem/nscp/releases/download/${Version}/NSCP-${Version}-${Arch}.msi" }
Write-Host "● Fetching MSI from URL: $MsiUrl"

# Generate a random password for the web interface
$WebPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 16 | ForEach-Object { [char]$_ })
Write-Host "● Generated web interface password."

# Fleet enrollment, when asked for, is handed to the MSI as properties so the
# host is enrolled by the installer itself (FLEET_SERVER/FLEET_TOKEN, 0.16+).
# An older MSI silently ignores unknown properties, which would leave the token
# unused - the step after the install detects that and falls back to
# `nscp enroll`. The CA, if any, has to be on disk before msiexec runs.
$remoteCaFile = 'C:\temp\fleet-ca.pem'
$caScript = ""
$msiFleetArgs = ""
if ($FleetServer) {
    Write-Host "● Fleet enrollment: $FleetServer (token from the fleet server, not shown)"
    $fleetProps = @("FLEET_SERVER=$FleetServer", "FLEET_TOKEN=$FleetToken")
    if ($FleetInsecure -or $FleetNoVerify) { $fleetProps += "FLEET_INSECURE=1" }
    if ($FleetNoVerify) { $fleetProps += "FLEET_VERIFY_MODE=none" }
    if ($FleetCaFile) {
        $fleetProps += "FLEET_CA=$remoteCaFile"
        $caPem = (Get-Content -Path $FleetCaFile -Raw).TrimEnd()
        $caScript = @"
Set-Content -Path '$remoteCaFile' -Encoding ascii -Value @'
$caPem
'@
"@
    }
    # Emitted into the remote script as extra elements of the argument array.
    $msiFleetArgs = "," + (($fleetProps | ForEach-Object { "'$_'" }) -join ",")
}

$scriptBlock = @"
New-Item -ItemType Directory -Path 'C:\temp' -Force
$caScript
Invoke-WebRequest -Uri '$($MsiUrl)' -OutFile 'C:\temp\installer.msi'
# Pass the arguments as an array (not one string) so a property value is quoted
# as a single argument, and check the exit code: a fleet enrollment that fails
# fails the install, and without this the run would sail on to a "web install"
# against a product that was never installed.
`$msiArgs = @('/i','C:\temp\installer.msi','/qn','/l*v','C:\temp\install.log'$msiFleetArgs)
`$msi = Start-Process msiexec.exe -ArgumentList `$msiArgs -Wait -PassThru
# 3010 is "installed, reboot required", which is still a successful install.
if (`$msi.ExitCode -ne 0 -and `$msi.ExitCode -ne 3010) {
    Get-Content 'C:\temp\install.log' -Tail 40 -ErrorAction SilentlyContinue
    throw "msiexec failed with exit code `$(`$msi.ExitCode) (see C:\temp\install.log on the VM)"
}
"@
$result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock
if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}
$Result.Value | ForEach-Object { $_.Message }
# RunCommand reports "Succeeded" for a script that threw; the error text is in
# the stderr stream, so look at the output as well before moving on.
if (($result.Value | ForEach-Object { $_.Message }) -match 'msiexec failed with exit code') {
    Write-Error "❌ The MSI install failed on the VM (see the output above)."
    exit 1
}

if ($FleetServer) {
    Write-Host "● Verifying fleet enrollment on VM '$VmName'..."
    $enrollArgs = @("--server", "'$FleetServer'", "--token", "'$FleetToken'")
    if ($FleetInsecure -or $FleetNoVerify) { $enrollArgs += "--insecure" }
    if ($FleetNoVerify) { $enrollArgs += @("--verify", "none") }
    if ($FleetCaFile) { $enrollArgs += @("--ca", "'$remoteCaFile'") }
    $scriptBlock = @"
`$state = 'C:\Program Files\NSClient++\security\agent-state.json'
if (Test-Path `$state) {
    Write-Output "FLEET: enrolled by the installer (`$state)"
} else {
    # No manifest means this MSI predates FLEET_SERVER/FLEET_TOKEN and dropped
    # them on the floor, so the bootstrap token is still unused: enroll from the
    # command line instead. (Had the installer tried and failed, it would have
    # failed the install above and we would never get here.)
    Write-Output "FLEET: the installer did not enroll (MSI without fleet support?) - falling back to 'nscp enroll'"
    & 'C:\Program Files\NSClient++\nscp.exe' enroll $($enrollArgs -join ' ')
    if (`$LASTEXITCODE -ne 0) { throw "nscp enroll failed with exit code `$LASTEXITCODE" }
    if (-not (Test-Path `$state)) { throw "nscp enroll reported success but `$state does not exist" }
    Write-Output "FLEET: enrolled via nscp enroll (`$state)"
}
"@
    $result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $scriptBlock
    if ($result.Status -ne "Succeeded") {
        Write-Error "❌ Failed to verify fleet enrollment on VM. Status: $($result.Status)"
        exit 1
    }
    $messages = @($result.Value | ForEach-Object { $_.Message })
    $messages | ForEach-Object { $_ }
    if (-not ($messages -match 'FLEET: enrolled')) {
        Write-Error "❌ The machine did not enroll with $FleetServer (see the output above)."
        exit 1
    }
    Write-Host "✅ Enrolled with the fleet server."
}

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
# NRDPClient and Scheduler are activated here even though nothing uses them
# yet: a fleet bundle that *enables* a module only writes it into fleet.ini, and
# the delayed reload that follows re-reads settings for the plugins already
# loaded - it does not load a newly enabled one. Without this the Nagios bundle
# applies, the host reports "in sync", and nothing is ever submitted until the
# service happens to restart. Loading them up front costs nothing (neither does
# anything without configuration) and keeps the monitoring flow turn-key.
& 'C:\Program Files\NSClient++\nscp.exe' settings --activate-module NRDPClient
& 'C:\Program Files\NSClient++\nscp.exe' settings --activate-module Scheduler

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

if ($SkipVersionCheck) {
    # -PackageUrl without a -Version to compare against (a local build's version
    # is whatever it is), so just report what got installed.
    Write-Host "●️ Skipping the version check (-SkipVersionCheck); installed version:"
    $result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName `
        -ScriptString "& 'C:\Program Files\NSClient++\nscp.exe' --version"
    $result.Value | ForEach-Object { $_.Message }
}
else {

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

}

$vmPublicIp = (Get-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName).IpAddress

# Save credentials to .vm.pwd file
if (-not $PwdFile) { $PwdFile = Join-Path (Split-Path $PSScriptRoot -Parent) ".vm.pwd" }
$fleetLine = if ($FleetServer) { "`nFleet Server:   $FleetServer" } else { "" }
@"
VM Name:        $VmName
Resource Group: $ResourceGroupName
Public IP:      $vmPublicIp
RDP:            $vmPublicIp:3389
Admin Username: $AdminUsername
Admin Password: $AdminPassword
Web URL:        https://$($vmPublicIp):8443
Web Password:   $WebPassword$fleetLine
"@ | Set-Content -Path $pwdFile -Force
Write-Host "● Credentials saved to $pwdFile"

Write-Host "✅ Script finished! VM '$VmName' is deployed and NSCP has been installed."
Write-Host "Connect via RDP: $vmPublicIp"
Write-Host "Web interface: https://$($vmPublicIp):8443"
Write-Host "Web password: $WebPassword"
Write-Host ""
Write-Host "Run the acceptance suite against it with:"
Write-Host "  ./build/powershell/run-tests.ps1 -VmName $VmName -Os windows"
