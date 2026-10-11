<#
.SYNOPSIS
    Helpers for the role-based estate: the role catalog, the machine budget,
    run folders, the install-fragment wrappers the setup scripts run on a VM,
    and the chunked fetch that pulls files off a VM. Dot-source it; it defines
    functions and does nothing on its own:

        . "$PSScriptRoot/estate/estate-api.ps1"
        $catalog = Get-EstateRoleCatalog
        $plan = ConvertFrom-RoleSpec -Spec "mysql=1,base=2" -Catalog $catalog

.DESCRIPTION
    A role is one folder under estate/roles/: a role.json descriptor, an install
    fragment per OS it supports, and the checks it is verified with (see
    README.md, "Role-based estate"). Nothing in here knows about a particular
    role.

    Works on Windows PowerShell 5.1 and PowerShell 7+, like fleet-api.ps1.
#>

$script:EstateDir = $PSScriptRoot
$script:EstateOsFamilies = @("windows", "ubuntu", "rocky")
$script:EstateExpectStates = @("OK", "WARNING", "CRITICAL", "UNKNOWN", "any")

function Get-EstateRolesDir {
    return (Join-Path $script:EstateDir "roles")
}

# Read and validate one role.json. Everything a later stage relies on is checked
# here, so a typo in a descriptor fails the run before any machine is created
# rather than ten minutes into it.
function Read-EstateRole {
    param([Parameter(Mandatory)][string]$Path)
    $dir = Split-Path -Parent $Path
    try { $role = Get-Content -Path $Path -Raw | ConvertFrom-Json }
    catch { throw "$Path is not valid JSON: $($_.Exception.Message)" }

    $name = "$($role.name)"
    $folder = Split-Path -Leaf $dir
    if ($name -ne $folder) { throw "$Path : name '$name' must match its folder '$folder'." }
    if ($name -notmatch '^[a-z][a-z0-9]*$') {
        throw "$Path : name '$name' must be lowercase letters and digits (it becomes the machine-name prefix and a tag value)."
    }

    $os = @($role.os | ForEach-Object { "$_" })
    if ($os.Count -eq 0) { throw "$Path : 'os' must list at least one of $($script:EstateOsFamilies -join ', ')." }
    foreach ($o in $os) {
        if ($script:EstateOsFamilies -notcontains $o) { throw "$Path : unknown os '$o' (known: $($script:EstateOsFamilies -join ', '))." }
    }
    # Machine names are <role>-<site>-<nn>: the role plus seven characters. A
    # Windows computer name is capped at 15, so a role that runs on Windows
    # gets at most eight.
    if ($os -contains "windows" -and $name.Length -gt 8) {
        throw "$Path : '$name' is too long for a Windows role (8 characters at most; the computer name is capped at 15)."
    }

    $machines = 1
    if ($null -ne $role.machines) { $machines = [int]$role.machines }
    if ($machines -lt 1) { throw "$Path : 'machines' must be 1 or more." }
    $vcpus = 1
    if ($null -ne $role.vcpus) { $vcpus = [int]$role.vcpus }
    if ($vcpus -lt 1) { throw "$Path : 'vcpus' must be 1 or more." }

    $modules = @($role.modules | Where-Object { $_ } | ForEach-Object { "$_" })
    foreach ($m in $modules) {
        if ($m -notmatch '^[A-Za-z0-9_]+$') { throw "$Path : module '$m' is not a plain module name." }
    }

    $install = @{}
    if ($role.install) {
        foreach ($p in $role.install.PSObject.Properties) {
            if ($os -notcontains $p.Name) { throw "$Path : install fragment for '$($p.Name)', which is not in 'os'." }
            $fragment = Join-Path $dir "$($p.Value)"
            if (-not (Test-Path $fragment)) { throw "$Path : install fragment '$($p.Value)' does not exist." }
            $install[$p.Name] = (Resolve-Path $fragment).Path
        }
    }

    $image = $null
    if ($role.image) {
        if ($os.Count -ne 1 -or $os[0] -ne "windows") {
            throw "$Path : 'image' is only supported for a Windows-only role."
        }
        foreach ($k in @("publisher", "offer", "sku")) {
            if (-not "$($role.image.$k)") { throw "$Path : 'image' needs publisher, offer and sku." }
        }
        $version = "latest"
        if ($role.image.version) { $version = "$($role.image.version)" }
        $image = "$($role.image.publisher):$($role.image.offer):$($role.image.sku):$version"
    }

    $tags = [ordered]@{}
    if ($role.tag) {
        foreach ($p in $role.tag.PSObject.Properties) { $tags[$p.Name] = "$($p.Value)" }
    }

    $checks = @($role.checks | Where-Object { $_ } | ForEach-Object {
            $c = $_
            $command = "$($c.command)"
            if ($command -notmatch '^[A-Za-z0-9_]+$') { throw "$Path : check command '$command' is not a plain command name." }
            $expect = "OK"
            if ($c.expect) { $expect = "$($c.expect)" }
            if ($script:EstateExpectStates -notcontains $expect) {
                throw "$Path : check '$command' expects '$expect' (one of $($script:EstateExpectStates -join ', '))."
            }
            # args travel as one key=value query parameter each, the way REST
            # passes them; an array repeats the key (several filter= clauses).
            $checkArgs = [ordered]@{}
            if ($c.args) {
                foreach ($p in $c.args.PSObject.Properties) {
                    $values = @($p.Value | ForEach-Object { "$_" })
                    if ($values.Count -eq 0) { throw "$Path : check '$command' argument '$($p.Name)' has no value." }
                    $checkArgs[$p.Name] = $values
                }
            }
            $checkOs = @($c.os | Where-Object { $_ } | ForEach-Object { "$_" })
            foreach ($o in $checkOs) {
                if ($o -ne "linux" -and $os -notcontains $o) {
                    throw "$Path : check '$command' is limited to os '$o', which is neither 'linux' nor in the role's 'os'."
                }
            }
            # "each": run the check once per machine of that role in the same
            # suite, with ${target.ip}, ${target.name} and ${target.password}
            # filled in by the jest suite (a hub checking its targets).
            $each = $null
            if ($c.each) {
                $each = "$($c.each)"
                if ($each -notmatch '^[a-z][a-z0-9]*$') { throw "$Path : check '$command' has 'each' = '$each', which is not a role name." }
            }
            [pscustomobject]@{
                Command = $command
                Args    = $checkArgs
                Expect  = $expect
                Perf    = @($c.perf | Where-Object { $_ } | ForEach-Object { "$_" })
                Os      = $checkOs
                Each    = $each
            }
        })

    # A bundle lands on the hosts its group selects, and the group selects on
    # the role's tags; with no tag the selector would be empty, which the fleet
    # server reads as "every host" - every machine in the tenant would get it.
    if ($role.bundle -and $tags.Count -eq 0) {
        throw "$Path : a role with a 'bundle' needs a 'tag' (its group selects on it; an empty selector matches every host)."
    }

    $logs = @{}
    if ($role.logs) {
        foreach ($p in $role.logs.PSObject.Properties) { $logs[$p.Name] = @($p.Value | ForEach-Object { "$_" }) }
    }

    $timeout = 10
    if ($role.installTimeoutMinutes) { $timeout = [int]$role.installTimeoutMinutes }

    # Settings written into the host's own nsclient.ini at install time. A
    # key there beats the same key from a bundle (fleet.ini is an include,
    # and local settings win), so a key the installer already wrote - the
    # MSI's NRPE preset - can only be changed here.
    $localSettings = [ordered]@{}
    if ($role.localSettings) {
        foreach ($section in $role.localSettings.PSObject.Properties) {
            $keys = [ordered]@{}
            foreach ($k in $section.Value.PSObject.Properties) { $keys[$k.Name] = "$($k.Value)" }
            $localSettings[$section.Name] = $keys
        }
        try { ConvertFrom-EstateLocalSettings -Encoded (ConvertTo-EstateLocalSettings -Settings $localSettings) | Out-Null }
        catch { throw "$Path : $($_.Exception.Message)" }
    }

    # Machines of other roles a suite of this one brings along (a hub's
    # targets); checked against the catalog once it is complete.
    $companions = [ordered]@{}
    if ($role.companions) {
        foreach ($p in $role.companions.PSObject.Properties) {
            if ($p.Name -eq $name) { throw "$Path : a role cannot be its own companion." }
            $n = [int]$p.Value
            if ($n -lt 1) { throw "$Path : companion '$($p.Name)' needs 1 or more machines." }
            $companions[$p.Name] = $n
        }
    }

    $result = [pscustomobject]@{
        Name                  = $name
        Description           = "$($role.description)"
        Dir                   = $dir
        Os                    = $os
        Machines              = $machines
        Vcpus                 = $vcpus
        VmSize                = "$($role.vmSize)"
        Image                 = $image
        Modules               = $modules
        Install               = $install
        InstallTimeoutMinutes = $timeout
        Tags                  = $tags
        Bundle                = $role.bundle
        Checks                = $checks
        Logs                  = $logs
        LocalSettings         = $localSettings
        Companions            = $companions
        # Only provisioned as another role's companion: `all` gives it no
        # suite of its own.
        CompanionOnly         = [bool]$role.companionOnly
    }
    # Compose the bundle once with a stand-in password: an unknown key or a
    # misspelt placeholder fails here, before any machine is created.
    if ($result.Bundle) {
        try { ConvertTo-EstateBundleConfig -Role $result -RolePassword "validate" | Out-Null }
        catch { throw "$Path : $($_.Exception.Message)" }
    }
    return $result
}

# Every role under estate/roles/, keyed by name.
function Get-EstateRoleCatalog {
    param([string]$RolesDir = (Get-EstateRolesDir))
    $catalog = [ordered]@{}
    foreach ($file in Get-ChildItem -Path $RolesDir -Filter role.json -Recurse -Depth 1 | Sort-Object FullName) {
        $role = Read-EstateRole -Path $file.FullName
        $catalog[$role.Name] = $role
    }
    if ($catalog.Count -eq 0) { throw "No roles found under $RolesDir." }
    # References between roles, now that every role is known.
    foreach ($role in $catalog.Values) {
        foreach ($c in $role.Companions.Keys) {
            if (-not $catalog.Contains($c)) { throw "Role '$($role.Name)': companion '$c' is not a role." }
            if ($catalog[$c].Companions.Count -gt 0) { throw "Role '$($role.Name)': companion '$c' has companions of its own." }
        }
        foreach ($check in $role.Checks) {
            if (-not $check.Each) { continue }
            if (-not $role.Companions.Contains($check.Each)) {
                throw "Role '$($role.Name)': check '$($check.Command)' runs for each '$($check.Each)', which is not one of its companions."
            }
        }
    }
    return $catalog
}

<#
.SYNOPSIS
    Turn "mysql=1,mssql=2" (or "mysql", meaning the role's own machine count)
    into a list of machines to create.
.OUTPUTS
    One object per machine: Role (the catalog entry), Os and Index (its
    position within the role, which round-robins the role's os list).
#>
function ConvertFrom-RoleSpec {
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$Spec,
        [Parameter(Mandatory)]$Catalog
    )
    $machines = [System.Collections.Generic.List[object]]::new()
    foreach ($part in ($Spec -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
        if ($part -notmatch '^([a-z][a-z0-9]*)(?:=(\d+))?$') {
            throw "'$part' is not a role request (expected <role> or <role>=<count>, e.g. mysql=1)."
        }
        $name = $Matches[1]
        if (-not $Catalog.Contains($name)) {
            throw "Unknown role '$name'. Known roles: $(@($Catalog.Keys) -join ', ')."
        }
        $role = $Catalog[$name]
        $count = $role.Machines
        if ($Matches[2]) { $count = [int]$Matches[2] }
        for ($i = 0; $i -lt $count; $i++) {
            $machines.Add([pscustomobject]@{ Role = $role; Os = $role.Os[$i % $role.Os.Count]; Index = $i })
        }
        # A role's companions come with it, whatever count it was asked for.
        foreach ($c in $role.Companions.Keys) {
            $companion = $Catalog[$c]
            for ($i = 0; $i -lt $role.Companions[$c]; $i++) {
                $machines.Add([pscustomobject]@{ Role = $companion; Os = $companion.Os[$i % $companion.Os.Count]; Index = $i })
            }
        }
    }
    # Enumerated: callers wrap the call in @() to keep a one-machine plan a list.
    return $machines.ToArray()
}

# The machine cap: refuse up front when this run plus what is already up
# would exceed it, rather than letting Azure refuse a VM create ten minutes in.
function Assert-EstateBudget {
    param(
        [Parameter(Mandatory)][int]$Requested,
        [int]$Existing = 0,
        [Parameter(Mandatory)][int]$MaxMachines
    )
    if ($Requested + $Existing -gt $MaxMachines) {
        $already = ""
        if ($Existing -gt 0) { $already = " plus the $Existing still in the manifest" }
        throw ("This run needs $Requested machine(s)$already, more than -MaxMachines $MaxMachines. " +
            "Tear down what is still up (-Destroy), ask for fewer machines, or raise -MaxMachines " +
            "if the regional vCPU quota allows it.")
    }
}

# Throwaway secrets for a throwaway estate: letters and digits only, so they
# survive every quoting layer between here and a config file on the VM.
function New-EstateSecret {
    param([int]$Length = 20)
    $chars = (65..90) + (97..122) + (48..57)
    return -join (1..$Length | ForEach-Object { [char](Get-Random -InputObject $chars) })
}

# .estate-runs/<timestamp>/ - every run's logs and results land here.
function New-EstateRunFolder {
    param([string]$Root = (Join-Path (Split-Path -Parent $script:EstateDir) ".estate-runs"))
    $path = Join-Path $Root (Get-Date -Format "yyyyMMdd-HHmmss")
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    return (Resolve-Path $path).Path
}

function ConvertTo-EstateBase64 {
    param([Parameter(Mandatory)][string]$Path)
    return [Convert]::ToBase64String([System.IO.File]::ReadAllBytes((Resolve-Path $Path).Path))
}

<#
.SYNOPSIS
    The RunCommand script that runs a role's install fragment on a Linux VM.
.DESCRIPTION
    The fragment travels base64-encoded, so nothing in it is ever interpreted
    by the shell that writes it to disk. It runs as root with ROLE_PASSWORD in
    its environment, under `timeout`, with all output captured to the install
    log. RunCommand only returns the tail of stdout, and reports "Succeeded"
    for a script that failed, so the last line is a marker carrying the exit
    code; the setup script decides on that.
#>
function New-LinuxRoleInstallScript {
    param(
        [Parameter(Mandatory)][string]$FragmentPath,
        [Parameter(Mandatory)][string]$RolePassword,
        [int]$TimeoutMinutes = 10,
        [string]$LogPath = "/var/log/nscp-role-install.log"
    )
    $b64 = ConvertTo-EstateBase64 -Path $FragmentPath
    return @"
#!/bin/bash
echo '$b64' | base64 -d > /tmp/nscp-role-install.sh
# Strip CRs: a fragment checked out on Windows must still run under bash.
sed -i 's/\r$//' /tmp/nscp-role-install.sh
chmod 700 /tmp/nscp-role-install.sh
ROLE_PASSWORD='$RolePassword' timeout ${TimeoutMinutes}m bash /tmp/nscp-role-install.sh > '$LogPath' 2>&1
rc=`$?
[ `$rc -eq 124 ] && echo "ROLE_INSTALL: timed out after $TimeoutMinutes minute(s)" >> '$LogPath'
tail -n 30 '$LogPath'
echo "ROLE_INSTALL: exit `$rc"
"@
}

# The Windows counterpart: the fragment runs in its own powershell.exe so its
# `exit` and its errors stay its own, with a wall-clock limit. That process
# writes the log itself (rather than Start-Process redirecting into it), so a
# fragment killed on timeout does not leave the log locked by this one.
function New-WindowsRoleInstallScript {
    param(
        [Parameter(Mandatory)][string]$FragmentPath,
        [Parameter(Mandatory)][string]$RolePassword,
        [int]$TimeoutMinutes = 10,
        [string]$LogPath = 'C:\nscp-role-install.log'
    )
    $b64 = ConvertTo-EstateBase64 -Path $FragmentPath
    $runner = @"
`$global:LASTEXITCODE = 0
try {
    & 'C:\temp\nscp-role-install.ps1' *>&1 | Out-File -FilePath '$LogPath' -Encoding utf8
    `$rc = `$LASTEXITCODE
}
catch {
    "`$_" | Out-File -FilePath '$LogPath' -Encoding utf8 -Append
    `$rc = 1
}
exit `$rc
"@
    $runnerB64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($runner))
    return @"
New-Item -ItemType Directory -Path 'C:\temp' -Force | Out-Null
[System.IO.File]::WriteAllBytes('C:\temp\nscp-role-install.ps1', [Convert]::FromBase64String('$b64'))
[System.IO.File]::WriteAllBytes('C:\temp\nscp-role-run.ps1', [Convert]::FromBase64String('$runnerB64'))
`$env:ROLE_PASSWORD = '$RolePassword'
`$p = Start-Process powershell.exe -PassThru -NoNewWindow ``
    -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:\temp\nscp-role-run.ps1'
# Touch the handle now: without it a -PassThru process reports no ExitCode.
`$null = `$p.Handle
if (-not `$p.WaitForExit($($TimeoutMinutes * 60000))) {
    Stop-Process -Id `$p.Id -Force -ErrorAction SilentlyContinue
    `$p.WaitForExit(10000) | Out-Null
    Add-Content -Path '$LogPath' -Value 'ROLE_INSTALL: timed out after $TimeoutMinutes minute(s)'
    `$rc = 124
} else {
    `$rc = `$p.ExitCode
}
Get-Content '$LogPath' -Tail 30 -ErrorAction SilentlyContinue
Write-Output "ROLE_INSTALL: exit `$rc"
"@
}

# Read the marker New-*RoleInstallScript ends with out of RunCommand's output.
# $null means the marker never arrived (the script died before its last line).
function Get-RoleInstallExitCode {
    param([string[]]$Messages)
    $hits = @($Messages -join "`n" | Select-String -Pattern 'ROLE_INSTALL: exit (\d+)' -AllMatches |
        ForEach-Object { $_.Matches } | ForEach-Object { [int]$_.Groups[1].Value })
    if ($hits.Count -eq 0) { return $null }
    return $hits[-1]
}

# Run a script on a VM via RunCommand, retrying transient Azure API errors
# (the same retry the setup scripts use).
function Invoke-EstateVmRun {
    param(
        [Parameter(Mandatory)][string]$ResourceGroupName,
        [Parameter(Mandatory)][string]$VmName,
        [Parameter(Mandatory)][ValidateSet("windows", "linux")][string]$Os,
        [Parameter(Mandatory)][string]$ScriptString,
        [int]$Retries = 3
    )
    $commandId = "RunShellScript"
    if ($Os -eq "windows") { $commandId = "RunPowerShellScript" }
    for ($attempt = 1; $attempt -le $Retries; $attempt++) {
        try {
            $result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName -VMName $VmName `
                -CommandId $commandId -ScriptString $ScriptString -ErrorAction Stop
            if ($result.Status -ne "Succeeded") { throw "RunCommand status $($result.Status)" }
            return $result
        }
        catch {
            Write-Warning "RunCommand on $VmName, attempt $attempt/$Retries failed: $($_.Exception.Message)"
            if ($attempt -eq $Retries) { throw }
            Start-Sleep -Seconds (15 * $attempt)
        }
    }
}

# RunCommand reports the two platforms differently. RunPowerShellScript
# returns one status per stream (ComponentStatus/StdOut/succeeded,
# ComponentStatus/StdErr/succeeded). RunShellScript returns a single
# ProvisioningState/succeeded status whose message is both streams in one:
#
#     Enable succeeded:
#     [stdout]
#     ...
#     [stderr]
#     ...
#
# so the Linux stdout has to be cut out of that text.
function Get-EstateVmStream {
    param($Result, [ValidateSet("stdout", "stderr")][string]$Stream)
    $code = if ($Stream -eq "stdout") { "ComponentStatus/StdOut/*" } else { "ComponentStatus/StdErr/*" }
    $perStream = @($Result.Value | Where-Object { $_.Code -like $code } | ForEach-Object { "$($_.Message)" })
    if ($perStream.Count -gt 0) { return ($perStream -join "`n") }
    $combined = @($Result.Value | ForEach-Object { "$($_.Message)" } | Where-Object { $_ -match '\[stdout\]' })
    if ($combined.Count -eq 0) { return "" }
    $text = $combined -join "`n"
    $outStart = $text.IndexOf("[stdout]")
    $errStart = $text.IndexOf("[stderr]", $outStart)
    if ($Stream -eq "stdout") {
        $from = $outStart + "[stdout]".Length
        $to = if ($errStart -ge 0) { $errStart } else { $text.Length }
        return $text.Substring($from, $to - $from).Trim()
    }
    if ($errStart -lt 0) { return "" }
    return $text.Substring($errStart + "[stderr]".Length).Trim()
}

function Get-EstateVmStdout {
    param($Result)
    return Get-EstateVmStream -Result $Result -Stream stdout
}

function Get-EstateVmStderr {
    param($Result)
    return Get-EstateVmStream -Result $Result -Stream stderr
}

<#
.SYNOPSIS
    Copy a file that a previous RunCommand left on the VM down to this machine.
.DESCRIPTION
    RunCommand returns at most the last 4 KB of stdout, so the file is base64
    encoded on the VM and read back 3000 characters per call - the transfer
    win/show-log.ps1 and linux/show-log.ps1 do, as a shared function. Every
    call costs 10-30 seconds, so callers compress first and fetch one archive
    rather than many small files.
#>
function Receive-EstateVmFile {
    param(
        [Parameter(Mandatory)][string]$ResourceGroupName,
        [Parameter(Mandatory)][string]$VmName,
        [Parameter(Mandatory)][ValidateSet("windows", "linux")][string]$Os,
        # The file on the VM to fetch.
        [Parameter(Mandatory)][string]$RemotePath,
        [Parameter(Mandatory)][string]$LocalPath,
        [int]$ChunkSize = 3000
    )
    if ($Os -eq "windows") {
        $prepare = @"
if (-not (Test-Path '$RemotePath')) { Write-Output 'FILE_NOT_FOUND'; return }
`$b64 = [Convert]::ToBase64String([System.IO.File]::ReadAllBytes('$RemotePath'))
[System.IO.File]::WriteAllText('$RemotePath.b64', `$b64)
Write-Output "LENGTH:`$(`$b64.Length)"
"@
    }
    else {
        $prepare = @"
#!/bin/bash
[ -f '$RemotePath' ] || { echo FILE_NOT_FOUND; exit 0; }
base64 -w0 '$RemotePath' > '$RemotePath.b64'
echo "LENGTH:`$(wc -c < '$RemotePath.b64')"
"@
    }
    $out = Get-EstateVmStdout (Invoke-EstateVmRun -ResourceGroupName $ResourceGroupName -VmName $VmName -Os $Os -ScriptString $prepare)
    if ($out -match 'FILE_NOT_FOUND') { throw "$RemotePath does not exist on $VmName." }
    if ($out -notmatch 'LENGTH:\s*(\d+)') { throw "Could not prepare $RemotePath on $VmName for transfer: $out" }
    $length = [int]$Matches[1]
    $chunks = [Math]::Ceiling($length / $ChunkSize)

    $b64 = [System.Text.StringBuilder]::new()
    for ($i = 0; $i -lt $chunks; $i++) {
        if ($Os -eq "windows") {
            $read = @"
`$s = [System.IO.File]::ReadAllText('$RemotePath.b64')
`$start = $($i * $ChunkSize)
Write-Output `$s.Substring(`$start, [Math]::Min($ChunkSize, `$s.Length - `$start))
"@
        }
        else {
            $read = "#!/bin/bash`ndd if='$RemotePath.b64' bs=$ChunkSize skip=$i count=1 2>/dev/null"
        }
        $chunk = Get-EstateVmStdout (Invoke-EstateVmRun -ResourceGroupName $ResourceGroupName -VmName $VmName -Os $Os -ScriptString $read)
        [void]$b64.Append(($chunk -replace '\s', ''))
    }
    if ($b64.Length -ne $length) {
        throw "Transfer of $RemotePath from $VmName came back short: $($b64.Length) of $length characters."
    }
    [System.IO.File]::WriteAllBytes($LocalPath, [Convert]::FromBase64String($b64.ToString()))
}

<#
.SYNOPSIS
    Poll the fleet server until every host has reached a stage, or time runs out.
.DESCRIPTION
    "enrolled" means the agent has joined (enrolled_at is set); "in_sync" means
    it has also applied its desired state and reported the matching hash back.
.OUTPUTS
    One object per host id: HostId, Reached (bool), Status, EnrolledAt.
#>
function Wait-EstateFleetHosts {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [Parameter(Mandatory)][string[]]$HostIds,
        [ValidateSet("enrolled", "in_sync")][string]$Until = "in_sync",
        [int]$WaitMinutes = 10,
        [switch]$SkipCertificateCheck,
        [scriptblock]$Log = { param($line) Write-Host $line }
    )
    $deadline = (Get-Date).AddMinutes($WaitMinutes)
    while ($true) {
        $states = foreach ($id in $HostIds) {
            $h = $null
            try { $h = Get-FleetHost -FleetServer $FleetServer -ApiKey $ApiKey -HostId $id -SkipCertificateCheck:$SkipCertificateCheck }
            catch { & $Log "   could not read fleet host ${id}: $($_.Exception.Message)" }
            $enrolled = $h -and $null -ne $h.enrolled_at
            $reached = $enrolled
            if ($Until -eq "in_sync") { $reached = $enrolled -and "$($h.status)" -eq "in_sync" }
            [pscustomobject]@{ HostId = $id; Reached = [bool]$reached; Status = "$($h.status)"; EnrolledAt = $h.enrolled_at }
        }
        $waiting = @($states | Where-Object { -not $_.Reached })
        if ($waiting.Count -eq 0 -or (Get-Date) -ge $deadline) { return $states }
        & $Log ("   [{0:HH:mm:ss}] waiting for {1} host(s) to be {2}: {3}" -f (Get-Date), $waiting.Count, $Until,
            (($waiting | ForEach-Object { "$($_.HostId) ($($_.Status))" }) -join ", "))
        Start-Sleep -Seconds 20
    }
}

# "1h 02m 13s", "12m 05s", "45s": how long a run or a stage took.
function Format-EstateDuration {
    param([Parameter(Mandatory)][double]$Seconds)
    $t = [TimeSpan]::FromSeconds([Math]::Round($Seconds))
    if ($t.TotalHours -ge 1) { return "{0}h {1:00}m {2:00}s" -f [int][Math]::Floor($t.TotalHours), $t.Minutes, $t.Seconds }
    if ($t.TotalMinutes -ge 1) { return "{0}m {1:00}s" -f $t.Minutes, $t.Seconds }
    return "{0}s" -f $t.Seconds
}

# --- install-time settings --------------------------------------------------------

# A role's "localSettings" travel to a setup script as one base64 JSON
# argument (it runs as a child process through -File).
function ConvertTo-EstateLocalSettings {
    param([Parameter(Mandatory)]$Settings)
    $json = ConvertTo-Json -InputObject $Settings -Depth 4 -Compress
    return [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($json))
}

<#
.SYNOPSIS
    Decode -LocalSettings back into Path/Key/Value rows, refusing anything
    that would not survive being quoted into a remote script.
.DESCRIPTION
    The values end up between single quotes in a PowerShell or bash script
    on the VM, so a single quote, a backtick or a line break is refused rather
    than escaped: no setting a role needs contains one.
#>
function ConvertFrom-EstateLocalSettings {
    param([Parameter(Mandatory)][string]$Encoded)
    $settings = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Encoded)) | ConvertFrom-Json
    $rows = foreach ($section in $settings.PSObject.Properties) {
        if ($section.Name -notmatch '^/[A-Za-z0-9_./ -]+$') { throw "localSettings: '$($section.Name)' is not a settings path (e.g. /settings/NRPE/server)." }
        foreach ($k in $section.Value.PSObject.Properties) {
            if ($k.Name -notmatch '^[A-Za-z0-9_. -]+$') { throw "localSettings: '$($k.Name)' under $($section.Name) is not a plain key." }
            $value = "$($k.Value)"
            if ($value -match "['``\r\n]") { throw "localSettings: the value of $($section.Name)/$($k.Name) contains a quote, backtick or line break." }
            [pscustomobject]@{ Path = $section.Name; Key = $k.Name; Value = $value }
        }
    }
    return @($rows)
}

<#
.SYNOPSIS
    The lines that write -LocalSettings rows on the VM and read each one back.
.DESCRIPTION
    `nscp settings --set` is not trusted to report a failure (the Linux
    scripts have to `|| true` the released binary's exit codes, and RunCommand
    reports "Succeeded" whatever the script exits with), so every key is read
    back with `--show` and a mismatch prints a LOCALSETTING-FAILED line, which
    Get-EstateLocalSettingFailures picks out of the output. The script carries
    on to the service restart either way, so the host is still up for log
    collection. A redacted (sensitive) key would read back as a mismatch; no
    role sets one locally. The value is single-quoted into a variable once and
    only expanded from there: ConvertFrom-EstateLocalSettings refuses a quote
    but not a $ or a ".
.PARAMETER Shell
    bash (sudo nscp) or powershell (the MSI's nscp.exe).
#>
function New-EstateLocalSettingsScript {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Rows,
        [Parameter(Mandatory)][ValidateSet("bash", "powershell")][string]$Shell
    )
    $lines = foreach ($r in $Rows) {
        $target = "--path '$($r.Path)' --key '$($r.Key)'"
        $what = "[$($r.Path)] $($r.Key)"
        if ($Shell -eq "bash") {
            "want='$($r.Value)'"
            "sudo nscp settings $target --set `"`$want`" || true"
            "v=`"`$(sudo nscp settings $target --show 2>/dev/null | tr -d '\r')`" || true"
            "printf '%s\n' `"`$v`" | grep -qxF -- `"`$want`" || echo `"LOCALSETTING-FAILED: $what reads back as '`$v', not '`$want'`""
        }
        else {
            "`$want = '$($r.Value)'"
            "& 'C:\Program Files\NSClient++\nscp.exe' settings $target --set `$want"
            "`$v = @(& 'C:\Program Files\NSClient++\nscp.exe' settings $target --show 2>`$null | ForEach-Object { `"`$_`".Trim() })"
            "if (`$v -notcontains `$want) { Write-Output `"LOCALSETTING-FAILED: $what reads back as '`$(`$v -join ' ')', not '`$want'`" }"
        }
    }
    return (@($lines) -join "`n")
}

# The LOCALSETTING-FAILED lines New-EstateLocalSettingsScript printed, if any.
function Get-EstateLocalSettingFailures {
    param([AllowEmptyCollection()][string[]]$Messages)
    return @($Messages | ForEach-Object { "$_" -split "`r?`n" } |
            Where-Object { $_ -match '^LOCALSETTING-FAILED: ' } |
            ForEach-Object { $_ -replace '^LOCALSETTING-FAILED: ', '' })
}

# --- role bundles ---------------------------------------------------------------

<#
.SYNOPSIS
    The config_json a role's bundle is composed from: its modules enabled, its
    "bundle.settings" with ${ROLE_PASSWORD} substituted, and
    ${ESTATE_IPS:<role>} with the public IPs of that role's machines in
    -Machines (the manifest), comma-separated: an allow-list for a server
    that only the hub may reach.
.DESCRIPTION
    The nested object path is the INI section path the agent renders into
    fleet.ini (libs/onboarding/sync.cpp), so { "settings": { "mysql": { ... } } }
    becomes [/settings/mysql]. The modules are already active from install
    time; enabling them again is a no-op there, and is what makes the bundle
    work on a host that was enrolled some other way.
#>
function ConvertTo-EstateBundleConfig {
    param(
        [Parameter(Mandatory)]$Role,
        [Parameter(Mandatory)][string]$RolePassword,
        # The manifest; without it (validating a descriptor) every
        # ${ESTATE_IPS:...} becomes a documentation address.
        [object[]]$Machines = $null
    )
    $bundle = $Role.Bundle
    foreach ($p in $bundle.PSObject.Properties) {
        if (@("settings") -notcontains $p.Name) {
            throw "Role '$($Role.Name)': unknown bundle key '$($p.Name)' (only 'settings'; modules come from the role's 'modules')."
        }
    }
    $modules = [ordered]@{}
    foreach ($m in $Role.Modules) { $modules[$m] = "enabled" }
    $config = [ordered]@{ modules = $modules }
    if ($bundle.settings) {
        # Through JSON text, so the placeholder is replaced at any depth. The
        # password is letters and digits (New-EstateSecret): nothing to escape.
        $json = ConvertTo-Json -InputObject $bundle.settings -Depth 16 -Compress
        $json = $json.Replace('${ROLE_PASSWORD}', $RolePassword)
        foreach ($wanted in @([regex]::Matches($json, '\$\{ESTATE_IPS:([a-z][a-z0-9]*)\}') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)) {
            $ips = @("192.0.2.1")
            if ($null -ne $Machines) {
                $ips = @($Machines | Where-Object { "$($_.Role)" -eq $wanted -and $_.PublicIp } | ForEach-Object { "$($_.PublicIp)" })
            }
            # Nothing to allow would leave the key empty, which a server reads
            # as "reject everyone" - say so rather than ship it.
            if ($ips.Count -eq 0) {
                throw "Role '$($Role.Name)': `${ESTATE_IPS:$wanted} has no '$wanted' machine with a public IP in the manifest."
            }
            $json = $json.Replace("`${ESTATE_IPS:$wanted}", ($ips -join ","))
        }
        if ($json -match '\$\{[A-Za-z_:]+\}') {
            throw "Role '$($Role.Name)': bundle settings use $($Matches[0]); only `${ROLE_PASSWORD} and `${ESTATE_IPS:<role>} are substituted."
        }
        $config.settings = $json | ConvertFrom-Json
    }
    return $config
}

# One password per role in a manifest: the role's bundle carries it to every
# machine of the role, so they must all have been given the same one.
function Get-EstateRolePassword {
    param(
        [Parameter(Mandatory)][string]$RoleName,
        [Parameter(Mandatory)]$Machines
    )
    $passwords = @($Machines | Where-Object { "$($_.Role)" -eq $RoleName -and $_.RolePassword } |
        ForEach-Object { "$($_.RolePassword)" } | Sort-Object -Unique)
    if ($passwords.Count -eq 0) { throw "No machine of role '$RoleName' in the manifest records a role password." }
    if ($passwords.Count -gt 1) {
        throw ("The '$RoleName' machines in the manifest carry $($passwords.Count) different role passwords, " +
            "but one bundle serves them all. They were provisioned by an older provision-fleet-machines.ps1; " +
            "tear them down and provision again.")
    }
    return $passwords[0]
}
