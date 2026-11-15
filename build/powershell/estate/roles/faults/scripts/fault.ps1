# Shipped by the `faults` role bundle and run by the agent (as SYSTEM) through
# fault.bat: switches one misbehaviour of the Acme Agent the role installed
# (install-windows.ps1) on, or every one off.
#
#   cpu      acme-indexer spinning one thread per core
#   memory   acme-indexer holding all but ~8% of the physical memory
#   disk     C:\ProgramData\Acme\cache\index.bin, filling C: to ~93%
#   service  acme-sync crash-loops until `clear`
#   clear    all of the above undone
#
# A started fault is created through WMI, outside the agent's process tree,
# so the agent gets its answer at once and does not wait on it; each one also
# ends by itself after an hour. Prints a Nagios line: OK when the fault is on
# (or off, for clear), UNKNOWN when it could not be.
param([string]$Fault = "")

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Join-Path $env:ProgramFiles 'Acme'
$data = Join-Path $env:ProgramData 'Acme'
$indexer = Join-Path $root 'acme-indexer.exe'
$cacheFile = Join-Path $data 'cache\index.bin'
$trigger = Join-Path $data 'sync.fail'

function Start-Detached([string]$CommandLine) {
    $r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $CommandLine }
    if ($r.ReturnValue -ne 0) { throw "Win32_Process.Create returned $($r.ReturnValue) for $CommandLine" }
}

try {
    if (-not (Test-Path $indexer)) { throw "the Acme Agent is not installed ($root)" }
    switch ($Fault) {
        'cpu' {
            Start-Detached "`"$indexer`" cpu"
            Write-Output "OK: acme-indexer started on $([Environment]::ProcessorCount) core(s)"
        }
        'memory' {
            $os = Get-CimInstance Win32_OperatingSystem
            $total = [long]$os.TotalVisibleMemorySize * 1KB
            $free = [long]$os.FreePhysicalMemory * 1KB
            $bytes = [long]($free - $total * 0.08)
            if ($bytes -le 0) { throw "only $([long]($free / 1MB)) MiB free of $([long]($total / 1MB)) MiB: nothing left to take" }
            Start-Detached "`"$indexer`" memory $bytes"
            Write-Output "OK: acme-indexer is taking $([long]($bytes / 1MB)) MiB"
        }
        'disk' {
            $disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'"
            $size = [long]$disk.Size
            $used = $size - [long]$disk.FreeSpace
            $want = [long]($size * 0.93 - $used)
            $cap = [long]$disk.FreeSpace - 1GB
            if ($want -gt $cap) { $want = $cap }
            if ($want -le 0) { throw "C: is already at $([int]($used * 100 / $size))%" }
            Remove-Item -Force -ErrorAction SilentlyContinue $cacheFile
            & fsutil.exe file createnew $cacheFile $want | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "fsutil file createnew of $want bytes exited $LASTEXITCODE" }
            Write-Output "OK: wrote $([long]($want / 1MB)) MiB to $cacheFile"
        }
        'service' {
            New-Item -ItemType File -Force -Path $trigger | Out-Null
            Write-Output "OK: acme-sync will crash until cleared"
        }
        'clear' {
            Get-Process -Name 'acme-indexer' -ErrorAction SilentlyContinue | Stop-Process -Force
            Remove-Item -Force -ErrorAction SilentlyContinue $cacheFile, $trigger
            # Its recovery action brings it back within 10 s anyway; this only
            # saves the wait when it is between two attempts.
            Start-Service -Name 'acme-sync' -ErrorAction SilentlyContinue
            Write-Output "OK: every Acme fault is cleared"
        }
        default { throw "usage: fault.bat cpu|memory|disk|service|clear" }
    }
    exit 0
}
catch {
    Write-Output "UNKNOWN: $($_.Exception.Message)"
    exit 3
}
