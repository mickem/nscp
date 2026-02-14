#Requires -Module Az.Compute

<#
.SYNOPSIS
    Used to deploy VM in azure with NSClient++
.DESCRIPTION
    This is not to be used, it is only for setting up personal test machines
.PARAMETER ResourceGroupName
    The name for the new resource group.
.PARAMETER VmName
    The name for the new virtual machine.
.PARAMETER Version
    The version of the software to install, specified as a URL to the MSI installer.
.PARAMETER Arch
    The architecture of the software to install (e.g., "x64" or "Win32").
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$VmName = "NSCP-Test",
    [string]$Version = "0.11.13",
    [string]$Arch = "x64"
)
Write-Host "ℹ️ Installing NSCP on VM '$VmName' in resource group '$ResourceGroupName'..."

$MsiUrl = "https://github.com/mickem/nscp/releases/download/${Version}/NSCP-${Version}-${Arch}.msi"
Write-Host "ℹ️ Fetching MSI from URL: $MsiUrl"
$scriptBlock = @"
New-Item -ItemType Directory -Path 'C:\temp' -Force
Invoke-WebRequest -Uri '$($MsiUrl)' -OutFile 'C:\temp\installer.msi'
Start-Process msiexec.exe -ArgumentList '/i C:\temp\installer.msi /l* c:\temp\install.log /qn' -Wait
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
