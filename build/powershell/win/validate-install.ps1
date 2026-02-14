<#
.SYNOPSIS
    Run nscp version to ensure that NSClient++ works on the VM
.DESCRIPTION
    This is not to be used, it is only for setting up personal test machines
.PARAMETER ResourceGroupName
    The name for the new resource group.
.PARAMETER VmName
    The name for the new virtual machine.
.PARAMETER Version
    The version to check for.
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$VmName = "NSCP-Test",
    [string]$Version = "0.9.14"
)

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


Write-Host "ℹ️️ Generating config on VM '$VmName' in resource group '$ResourceGroupName'..."
$scriptBlock = @"
if (-not (Test-Path -Path 'C:\temp')) {
    mkdir 'C:\temp'
}
# Temporary workaround for bug making it impossible to override ini file
if (Test-Path -Path 'C:\Program Files\NSClient++\nsclient.ini') {
    Remove-Item 'C:\Program Files\NSClient++\nsclient.ini'
}
if (Test-Path -Path 'C:\Program Files\NSClient++\boot.ini') {
    Remove-Item 'C:\Program Files\NSClient++\boot.ini'
}
if (Test-Path -Path 'C:\temp\nsclient.ini') {
    Remove-Item 'C:\temp\nsclient.ini'
}
`$output = & 'C:\Program Files\NSClient++\nscp.exe' settings --settings "ini://c:/temp/nsclient.ini" --load-all --update --add-defaults
Write-Output "NSClient++ command output: "
Write-Output "`$output"
if (Test-Path -Path 'C:\temp\nsclient.ini') {
    Write-Output "SUCCESS: Config file generated at C:\temp\nsclient.ini"
} else {
    dir c:\temp
    throw "FAILURE: Config file was not generated."
}
"@
$Result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
    -VMName $VmName `
    -CommandId 'RunPowerShellScript' `
    -ScriptString $scriptBlock

$value0 = $result.Value[0].Message
$value1 = $result.Value[1].Message

if ($result.Status -ne "Succeeded") {
    Write-Error "❌ Failed to run command on VM. Status: $($result.Status)"
    exit 1
}
if ($value0 -match "SUCCESS: ")
{
    Write-Host "✅ Configuration generated."
} elseif ($value1 -match "SUCCESS: ") {
    Write-Host "✅ Configuration generated."
} else {
    Write-Error "❌ Configuration generation failed. Output was:"
    $result.Value | ForEach-Object { $_.Message }
    exit 1
}
Write-Host "✅ Correct version installed!"


