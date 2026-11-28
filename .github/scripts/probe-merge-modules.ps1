# Temporary diagnostic for #1776: print every merge module and every
# redistributable runtime folder Visual Studio has on this machine.
$roots = @(
  'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC',
  'C:\Program Files (x86)\Common Files\Merge Modules',
  'C:\Program Files\Common Files\Merge Modules'
)
foreach ($root in $roots) {
  Write-Host "== $root"
  if (-not (Test-Path $root)) { Write-Host "   (missing)"; continue }
  Get-ChildItem $root -Directory -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "   dir: $($_.Name)" }
  Write-Host "-- merge modules"
  Get-ChildItem $root -Recurse -Filter *.msm -ErrorAction SilentlyContinue |
    ForEach-Object { "   {0,10} {1} {2}" -f $_.Length, $_.LastWriteTime.ToString('yyyy-MM-dd'), $_.FullName.Substring($root.Length) }
  Write-Host "-- runtime DLLs"
  Get-ChildItem $root -Recurse -Filter vcruntime140.dll -ErrorAction SilentlyContinue |
    ForEach-Object { "   {0,-18} {1}" -f $_.VersionInfo.FileVersion, $_.FullName.Substring($root.Length) }
  Write-Host "-- redistributable installers"
  Get-ChildItem $root -Recurse -Filter *redist*.exe -ErrorAction SilentlyContinue |
    ForEach-Object { "   {0,-18} {1}" -f $_.VersionInfo.FileVersion, $_.FullName.Substring($root.Length) }
}
Write-Host "== toolsets"
Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC' -Directory -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "   $($_.Name)" }
exit 0
