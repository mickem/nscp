$ErrorActionPreference = 'Stop'

# NSClient++ uses a stable UpgradeCode but the MSI ProductCode varies across
# versions, so we look up the installed product by its display name in the
# uninstall registry hive.
$packageName = 'nscp'
$displayName = 'NSClient++*'

[array]$keys = Get-UninstallRegistryKey -SoftwareName $displayName

if ($keys.Count -eq 0) {
  Write-Warning "$packageName does not appear to be installed."
  return
}

foreach ($key in $keys) {
  $silentArgs    = "$($key.PSChildName) /qn /norestart"
  $validExitCodes = @(0, 3010, 1605, 1614, 1641)

  Uninstall-ChocolateyPackage -PackageName $packageName `
    -FileType 'msi' `
    -SilentArgs $silentArgs `
    -ValidExitCodes $validExitCodes `
    -File ''
}
