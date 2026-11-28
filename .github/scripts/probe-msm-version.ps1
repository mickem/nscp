# Temporary diagnostic for #1776: print the runtime version inside each CRT
# merge module Visual Studio has on this machine.
$root = 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC'
$installer = New-Object -ComObject WindowsInstaller.Installer
function Invoke-Com($obj, $name, $kind, $callArgs) {
  return $obj.GetType().InvokeMember($name, $kind, $null, $obj, $callArgs)
}
Get-ChildItem $root -Recurse -Filter 'Microsoft_VC*_CRT_x*.msm' | Where-Object { $_.Name -notmatch 'Debug' } | ForEach-Object {
  Write-Host "== $($_.FullName.Substring($root.Length))"
  try {
    $db = Invoke-Com $installer 'OpenDatabase' 'InvokeMethod' @($_.FullName, 0)
    $view = Invoke-Com $db 'OpenView' 'InvokeMethod' @('SELECT `FileName`, `Version` FROM `File`')
    $null = Invoke-Com $view 'Execute' 'InvokeMethod' $null
    while ($true) {
      $rec = Invoke-Com $view 'Fetch' 'InvokeMethod' $null
      if (-not $rec) { break }
      $name = Invoke-Com $rec 'StringData' 'GetProperty' @(1)
      $version = Invoke-Com $rec 'StringData' 'GetProperty' @(2)
      "   {0,-16} {1}" -f $version, $name
    }
  } catch {
    Write-Host "   ! $($_.Exception.Message)"
  }
}
exit 0
