# Temporary diagnostic for #1776, runs INSIDE a servercore container: install
# a published MSI on a machine with no Visual C++ runtime, see what lands in
# System32, then uninstall and see what is left.
param([string]$Msi = 'C:\work\NSCP.msi')
$ErrorActionPreference = 'Continue'

function Show-Runtime($label) {
  Write-Host "== runtime: $label"
  $found = $false
  foreach ($dir in 'C:\Windows\System32', 'C:\Windows\SysWOW64') {
    foreach ($pattern in 'vcruntime140*.dll', 'msvcp140*.dll') {
      Get-ChildItem $dir -Filter $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        $found = $true
        "   {0,-16} {1}" -f ($_.VersionInfo.FileVersion -split ' ')[0], $_.FullName
      }
    }
  }
  if (-not $found) { Write-Host "   (none)" }
  $shared = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\SharedDLLs' -ErrorAction SilentlyContinue
  if ($shared) {
    $shared.PSObject.Properties | Where-Object { $_.Name -match 'vcruntime140\.dll$' } |
      ForEach-Object { "   SharedDLLs count {0} for {1}" -f $_.Value, $_.Name }
  }
}

function Run-Msi($label, [string[]]$msiArgs, $log) {
  Write-Host "== msiexec: $label -> $($msiArgs -join ' ')"
  $p = Start-Process msiexec.exe -ArgumentList ($msiArgs + @('/qn', '/l*v', $log)) -Wait -PassThru
  Write-Host "   exit code: $($p.ExitCode)"
  if ($p.ExitCode -ne 0 -and (Test-Path $log)) {
    Write-Host "-- log lines around the failure"
    $lines = Get-Content $log
    $hits = $lines | Select-String -Pattern 'Return value 3|Error \d{4}|CustomAction .* returned actual error' | Select-Object -First 15
    $hits | ForEach-Object { "   " + $_.Line }
    Write-Host "-- last 25 log lines"
    $lines | Select-Object -Last 25 | ForEach-Object { "   " + $_ }
  }
  return $p.ExitCode
}

Write-Host "== OS"
cmd /c ver
Show-Runtime 'before install (expect none)'

$code = Run-Msi 'default install' @('/i', $Msi) 'C:\work\install1.log'
if ($code -ne 0) {
  $code = Run-Msi 'install without the firewall feature' @('/i', $Msi, 'ADDLOCAL=ALL', 'REMOVE=FirewallConfig') 'C:\work\install2.log'
}
Show-Runtime 'after install'

$nscp = 'C:\Program Files\NSClient++\nscp.exe'
Write-Host "== agent"
if (Test-Path $nscp) {
  & $nscp --version
  Write-Host "   nscp --version exit code: $LASTEXITCODE"
} else {
  Write-Host "   $nscp is missing"
}
Get-Service -Name nscp -ErrorAction SilentlyContinue | ForEach-Object { "   service {0}: {1}" -f $_.Name, $_.Status }

$null = Run-Msi 'uninstall' @('/x', $Msi) 'C:\work\uninstall.log'
Show-Runtime 'after uninstall (is the runtime removed when we were the only owner?)'
exit 0
