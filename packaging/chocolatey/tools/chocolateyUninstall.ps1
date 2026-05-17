$ErrorActionPreference = 'Stop'

# NSClient++ uses a stable UpgradeCode but the MSI ProductCode varies across
# versions, so we look up the installed product by its display name in the
# uninstall registry hive.
$packageName = 'nsclient'
$displayName = 'NSClient++*'

[array]$keys = Get-UninstallRegistryKey -SoftwareName $displayName

if ($keys.Count -eq 0) {
  Write-Warning "$packageName does not appear to be installed."
  return
}

foreach ($key in $keys) {
  $silentArgs    = "$($key.PSChildName) /qn /norestart"
  $validExitCodes = @(0, 3010, 1605, 1614, 1641)

  # -File is required by Uninstall-ChocolateyPackage but is intentionally
  # empty here: we uninstall by product code (passed in $silentArgs), not by
  # path to a downloaded MSI. Do not "tidy up" by removing this argument.
  Uninstall-ChocolateyPackage -PackageName $packageName `
    -FileType 'msi' `
    -SilentArgs $silentArgs `
    -ValidExitCodes $validExitCodes `
    -File ''
}
