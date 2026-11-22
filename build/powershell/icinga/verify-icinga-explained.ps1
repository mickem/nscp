<#
.SYNOPSIS
    Gate for the "Icinga alert emails that explain themselves" tutorial: break
    a check on every machine and require an explained notification for it.
.DESCRIPTION
    Step 7 of the tutorial, as a test. With the Icinga VM set up
    (setup-icinga-explained-alerts.ps1) and the agents configured
    (configure-explained-agents.ps1), it

      1. waits until every machine of the estate manifest is a host in Icinga
         whose services (explained-checks.json) hold a passive result, with
         the fault check OK - the baseline, tutorial Steps 3 and 4;
      2. tightens the fault check's threshold on every machine
         (configure-explained-agents.ps1 -BreakDisk), which makes it fail for
         real;
      3. waits until Icinga holds that check as a hard CRITICAL from the
         agent and has sent its problem notification;
      4. reads the notification mail from the mailbox on the Icinga VM
         (/var/mail/alerts, where a copy of every mail goes) and requires
         the tutorial's result: the "Why" section, written by
         `check_nsclient fleet explain` from inside the handler, with a
         "Based on" block that rests on the agent's own report of that check;
      5. puts the threshold back (always, also when a step above failed) and
         waits for the check to recover and for the recovery mail, which
         carries no explanation.

    No mail leaves the VM for this: the gate reads the local copy, so it
    works the same with or without a relay configured. Every explanation is
    printed, since what the model wrote is the interesting part. A run costs
    one model call per machine.

    The handler waits `vars.explain_settle` seconds before it asks, so that
    the agent's failure report is on the fleet server first. When the
    explanation still says that no reported check matched, the report lost
    that race: raise -ExplainSettle on the setup script.
.PARAMETER BaselineOnly
    Stop after step 1: nothing is broken and no model is called.
.PARAMETER WaitMinutes
    How long each wait may take (default 10): for the baseline, for the
    notification, and for the recovery.
.PARAMETER PwdFile
    Credentials file of the Icinga VM (default: ../.icinga.pwd).
.PARAMETER ManifestFile
    The estate manifest (default: ../.fleet-machines.json).
.EXAMPLE
    ./verify-icinga-explained.ps1
.EXAMPLE
    ./verify-icinga-explained.ps1 -BaselineOnly
#>
param(
    [switch]$BaselineOnly,
    [ValidateRange(1, 60)]
    [int]$WaitMinutes = 10,
    [string]$PwdFile = "",
    [string]$ManifestFile = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")
. (Join-Path $parentDir "estate/estate-api.ps1")

if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".icinga.pwd" }
if (-not $ManifestFile) { $ManifestFile = Join-Path $parentDir ".fleet-machines.json" }
$icinga = Read-IcingaPwdFile -Path $PwdFile
if (-not $icinga.IcingaUrl -or -not $icinga.AdminUser -or -not $icinga.VmName) {
    Write-Error "❌ '$PwdFile' is not the credentials file of an Icinga VM - run setup-icinga-machine.ps1 first."
    exit 1
}
if (-not $icinga.ExplainUser) {
    Write-Error "❌ '$PwdFile' has no explained-alerts API user - run setup-icinga-explained-alerts.ps1 first."
    exit 1
}
$machines = @(Read-FleetManifest -Path $ManifestFile)
if ($machines.Count -eq 0) {
    Write-Error "❌ No machines in '$ManifestFile' - provision the estate first (provision-fleet-machines.ps1)."
    exit 1
}
$hostNames = @($machines | ForEach-Object { "$($_.Name)".ToLowerInvariant() })
$catalog = Read-ExplainedCheckCatalog -Path (Join-Path $scriptDir "explained-checks.json")
$fault = $catalog.Fault
$api = New-IcingaApi -Url $icinga.IcingaUrl -User $icinga.AdminUser -Password $icinga.AdminPassword
$mailbox = "/var/mail/alerts"

Write-Host "Icinga server: $($api.Base)"
Write-Host "Hosts:         $($hostNames -join ', ') ($ManifestFile)"
Write-Host "Services:      $($catalog.ServiceNames -join ', '); the fault is '$fault'"

function Invoke-IcingaVm {
    param([Parameter(Mandatory)][string]$Script)
    $result = Invoke-EstateVmRun -ResourceGroupName $icinga.ResourceGroup -VmName $icinga.VmName -Os linux `
        -ScriptString ($Script -replace "`r", "")
    return Get-EstateVmStdout $result
}

# One service object, or $null while Icinga does not have it.
function Get-IcingaService {
    param([string]$HostName, [string]$Service)
    return (Invoke-IcingaApi -Api $api -Path "/objects/services/$HostName!$Service" | Select-Object -First 1)
}

# Only a result an agent submitted counts: Icinga's own "dummy" check answers
# "No data received" when nothing arrives, and that is not a baseline.
function Test-PassiveResult {
    param($Object)
    if (-not $Object) { return $false }
    $result = $Object.attrs.last_check_result
    return ($null -ne $result -and $result.active -eq $false)
}

# Poll -Probe (which returns the names still missing) until it returns none.
function Wait-Until {
    param([Parameter(Mandatory)][scriptblock]$Probe, [Parameter(Mandatory)][string]$What)
    $deadline = (Get-Date).AddMinutes($WaitMinutes)
    $missing = @()
    while ($true) {
        $missing = @(& $Probe)
        if ($missing.Count -eq 0) { return @() }
        if ((Get-Date) -ge $deadline) { return $missing }
        Write-Host "   waiting for ${What}: $($missing -join ', ')"
        Start-Sleep -Seconds 15
    }
}

# --- 1. the baseline ---------------------------------------------------------
$missing = Wait-Until -What "passive results" -Probe {
    foreach ($h in $hostNames) {
        $hostObject = Invoke-IcingaApi -Api $api -Path "/objects/hosts/$h" | Select-Object -First 1
        if (-not (Test-PassiveResult $hostObject)) { "$h/host_check" }
        foreach ($s in $catalog.ServiceNames) {
            $service = Get-IcingaService $h $s
            if (-not (Test-PassiveResult $service)) { "$h/$s" }
            elseif ($s -eq $fault -and [int]$service.attrs.state -ne 0) { "$h/$s (state $([int]$service.attrs.state), not OK)" }
        }
    }
}
if ($missing.Count -gt 0) {
    Write-Error (@(
            "❌ The baseline did not arrive within $WaitMinutes minute(s): $($missing -join ', ')",
            "   A check without a passive result means the agent is not submitting: look for 'icinga' in its nsclient.log",
            "   (the API user, the address and the CA come from configure-explained-agents.ps1), and check that the",
            "   machine can reach port 5665 on the Icinga VM."
        ) -join "`n")
    exit 1
}
Write-Host "✅ Every host submits $($catalog.ServiceNames -join ', ') and its host check; '$fault' is OK."
if ($BaselineOnly) { return }

# --- 2. to 5. break it, read the mail, put it back ---------------------------
$offset = 0
$out = Invoke-IcingaVm -Script "#!/bin/bash`necho `"MAILBOX-SIZE: `$(stat -c %s $mailbox 2>/dev/null || echo 0)`""
if ($out -match 'MAILBOX-SIZE:\s*(\d+)') { $offset = [long]$Matches[1] }
else {
    Write-Error "❌ Could not read $mailbox on '$($icinga.VmName)': $out"
    exit 1
}

$configure = Join-Path $scriptDir "configure-explained-agents.ps1"
$problems = @()
$broken = $false
try {
    $started = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $broken = $true
    & $configure -BreakDisk -PwdFile $PwdFile -ManifestFile $ManifestFile

    $missing = Wait-Until -What "a hard CRITICAL and its notification" -Probe {
        foreach ($h in $hostNames) {
            $service = Get-IcingaService $h $fault
            $result = if ($service) { $service.attrs.last_check_result } else { $null }
            if (-not $result -or $result.active -ne $false -or [int]$service.attrs.state -ne 2 -or [int]$service.attrs.state_type -ne 1) {
                "$h/$fault"
                continue
            }
            $notification = Invoke-IcingaApi -Api $api -Path "/objects/notifications/$h!$fault!mail-explained" | Select-Object -First 1
            if (-not $notification -or [double]$notification.attrs.last_problem_notification -lt $started) { "$h/$fault (notification)" }
        }
    }
    foreach ($m in $missing) { $problems += "$m did not become a hard CRITICAL with a notification within $WaitMinutes minute(s)" }

    # The handler sleeps (explain_settle), then may wait 150 s on the model,
    # so the mail trails the notification. Judged on the VM, on the whole
    # mail: RunCommand hands back only the last 4 KB of output.
    foreach ($h in $hostNames) {
        if ($missing -match "^$([regex]::Escape($h))/") { continue }
        $subject = "Subject: [PROBLEM] $fault on $h is CRITICAL"
        $read = @'
#!/bin/bash
subject='__SUBJECT__'
for i in $(seq 1 54); do
    tail -c +__FROM__ __MAILBOX__ 2>/dev/null | grep -qF "$subject" && break
    sleep 5
done
tail -c +__FROM__ __MAILBOX__ 2>/dev/null |
    awk -v subject="$subject" 'BEGIN { RS = "\nFrom " } index($0, subject) { message = $0 } END { printf "%s", message }' |
    sed '1,/^$/d' > /tmp/explained-mail.txt
if [ ! -s /tmp/explained-mail.txt ]; then echo "MAIL: missing"; exit 0; fi
flag() { if grep -q -- "$2" /tmp/explained-mail.txt; then echo "$1: yes"; else echo "$1: no"; fi; }
flag WHY '^---- Why (model-written, from fleet evidence) ----'
flag UNEXPLAINED '^No explanation available'
flag BASED-ON '^Based on'
flag UNMATCHED 'no reported check matched'
echo "MAIL-BEGIN"
if [ "$(wc -c < /tmp/explained-mail.txt)" -gt 3300 ]; then
    head -c 1300 /tmp/explained-mail.txt; printf '\n   [...]\n'; tail -c 1900 /tmp/explained-mail.txt
else
    cat /tmp/explained-mail.txt
fi
echo "MAIL-END"
'@
        $read = $read.Replace('__SUBJECT__', $subject).Replace('__FROM__', "$($offset + 1)").Replace('__MAILBOX__', $mailbox)
        $out = Invoke-IcingaVm -Script $read
        if ($out -match 'MAIL: missing' -or $out -notmatch 'MAIL-BEGIN') {
            $problems += "${h}: no '[PROBLEM] $fault on $h is CRITICAL' mail reached $mailbox"
            continue
        }
        $body = ($out -replace '(?s)^.*MAIL-BEGIN\r?\n', '') -replace '(?s)\r?\nMAIL-END.*$', ''
        Write-Host ""
        Write-Host "---- [PROBLEM] $fault on $h is CRITICAL ----"
        Write-Host $body
        Write-Host ""
        if ($out -notmatch 'WHY: yes') { $problems += "${h}: the mail has no 'Why' section - the handler did not ask for an explanation" }
        elseif ($out -match 'UNEXPLAINED: yes') { $problems += "${h}: the handler got no explanation (the mail says why, above)" }
        elseif ($out -notmatch 'BASED-ON: yes') { $problems += "${h}: the explanation has no 'Based on' block" }
        elseif ($out -match 'UNMATCHED: yes') { $problems += "${h}: the explanation does not rest on the agent's report of '$fault' - it had not reached the fleet server when the handler asked" }
        else { Write-Host "✅ ${h}: the notification explains itself, from the agent's own report of '$fault'." }
    }
}
catch {
    $problems += "the run stopped: $($_.Exception.Message)"
}
finally {
    if ($broken) {
        try { & $configure -RestoreDisk -PwdFile $PwdFile -ManifestFile $ManifestFile }
        catch { $problems += "'$fault' was NOT restored - run configure-explained-agents.ps1 -RestoreDisk: $($_.Exception.Message)" }
    }
}

# --- the recovery ------------------------------------------------------------
$missing = Wait-Until -What "the recovery" -Probe {
    foreach ($h in $hostNames) {
        $service = Get-IcingaService $h $fault
        if (-not (Test-PassiveResult $service) -or [int]$service.attrs.state -ne 0) { "$h/$fault" }
    }
}
foreach ($m in $missing) { $problems += "$m did not recover within $WaitMinutes minute(s)" }
if ($missing.Count -eq 0) {
    $read = @'
#!/bin/bash
for i in $(seq 1 12); do
    all=1
    for h in __HOSTS__; do
        tail -c +__FROM__ __MAILBOX__ 2>/dev/null | grep -qF "Subject: [RECOVERY] __FAULT__ on $h is OK" || all=0
    done
    [ "$all" = 1 ] && break
    sleep 5
done
for h in __HOSTS__; do
    if tail -c +__FROM__ __MAILBOX__ 2>/dev/null | grep -qF "Subject: [RECOVERY] __FAULT__ on $h is OK"; then echo "RECOVERY $h: yes"; else echo "RECOVERY $h: no"; fi
done
'@
    $read = $read.Replace('__HOSTS__', ($hostNames -join " ")).Replace('__FROM__', "$($offset + 1)").
    Replace('__MAILBOX__', $mailbox).Replace('__FAULT__', $fault)
    $out = Invoke-IcingaVm -Script $read
    foreach ($h in $hostNames) {
        if ($out -notmatch "RECOVERY $([regex]::Escape($h)): yes") { $problems += "${h}: no '[RECOVERY] $fault on $h is OK' mail reached $mailbox" }
    }
}

if ($problems.Count -gt 0) {
    Write-Error (@("❌ The explained-alerts flow did not hold:") + @($problems | ForEach-Object { "   $_" }) -join "`n")
    exit 1
}
Write-Host "✅ All $($hostNames.Count) host(s): '$fault' failed, was mailed with an explanation, and recovered."
