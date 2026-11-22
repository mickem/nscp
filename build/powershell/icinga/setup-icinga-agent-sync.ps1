#Requires -Module Az.Accounts
#Requires -Module Az.Compute

<#
.SYNOPSIS
    Prepare the Icinga VM for agent mode: the check command, templates and
    services NSClient++ agents are scheduled with, and a timer that registers
    every enrolled fleet host as an Icinga agent and signs its certificate.
.DESCRIPTION
    Stage 3a of the agent-mode Icinga flow (see README.md). The passive flow
    needs nothing on the Icinga VM per host, because the agents create their
    own objects through the REST API. In agent mode the master schedules the
    checks (`command_endpoint`, docs/docs/scenarios/icinga-agent.md), so the
    master has to hold an Endpoint, a Zone and a Host for every agent and has
    to sign its certificate. This script installs, on the VM
    setup-icinga-machine.ps1 created:

      - conf.d/fleet-agent-templates.conf: the "fleet-nscp-query" CheckCommand
        (ifw-api, any NSClient++ query with its arguments as written) and the
        "fleet-agent-host" / "fleet-agent-service" templates;
      - conf.d/fleet-agent-services.conf: one apply rule per entry of
        agent-checks.json, matching every host the timer registers;
      - fleet-icinga-agent-sync: a one-minute systemd timer that polls
        GET /api/hosts on the fleet server, renders Endpoint + Zone + Host for
        every enrolled host into conf.d/fleet-agents/, reloads Icinga when that
        changed, and signs the pending certificate requests of those hosts.

    It then records what the agents need to find and trust the master - its
    endpoint name and the SHA-256 fingerprint of its certificate - in
    .icinga.pwd, for add-icinga-agent-bundle.ps1.

    Safe to re-run: after editing agent-checks.json it rewrites the services
    and reloads Icinga.

    The timer signs any request made in the name of an enrolled fleet host,
    with no ticket. Fine for a throwaway test estate, not for anything else.
.PARAMETER FleetServer
    Fleet server url the VM polls. Defaults to $env:NSCLIENT_FLEET_SERVER, then
    to the "Fleet Server" line of the .icinga.pwd file.
.PARAMETER SyncApiKey
    The fleet API key the timer uses. It ends up in a root-only file on the VM,
    so use a view_only key. Defaults to $env:NSCLIENT_FLEET_SYNC_API_KEY, then
    (with a warning) to $env:NSCLIENT_FLEET_API_KEY.
.PARAMETER PwdFile
    The credentials file setup-icinga-machine.ps1 wrote (default:
    build/powershell/.icinga.pwd); names the VM and is given the master's
    endpoint name and fingerprint.
.PARAMETER ReplacePassive
    Let the timer delete a host the passive flow created through the API when
    a fleet host of the same name is registered as an agent (the two cannot
    both exist). Without it such a host is skipped and logged.
.PARAMETER FleetNoVerify
    The VM does not verify the fleet server certificate (self-signed test
    server).
.EXAMPLE
    $env:NSCLIENT_FLEET_SYNC_API_KEY = "nsk_..."   # a view_only key
    ./setup-icinga-agent-sync.ps1
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$SyncApiKey = "",
    [string]$PwdFile = "",
    [switch]$ReplacePassive,
    [switch]$FleetNoVerify
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")
Import-FleetConfig
if (-not $FleetServer) { $FleetServer = $env:NSCLIENT_FLEET_SERVER }

# --- the VM and its credentials ----------------------------------------------
if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".icinga.pwd" }
if (-not (Test-Path $PwdFile)) {
    Write-Error "❌ '$PwdFile' does not exist - run setup-icinga-machine.ps1 first."
    exit 1
}
$existing = @{}
foreach ($line in Get-Content -Path $PwdFile) {
    if ($line -match '^([^:]+):\s*(.*)$') { $existing[$Matches[1].Trim()] = $Matches[2].Trim() }
}
foreach ($key in "VM Name", "Resource Group", "Public IP", "Icinga Admin User", "Icinga Admin Password") {
    if (-not $existing[$key]) {
        Write-Error "❌ '$PwdFile' has no '$key' line - it was not written by setup-icinga-machine.ps1."
        exit 1
    }
}
$VmName = $existing["VM Name"]
$ResourceGroupName = $existing["Resource Group"]
if (-not $FleetServer) { $FleetServer = $existing["Fleet Server"] }
if (-not $FleetServer) {
    Write-Error "❌ No fleet server: pass -FleetServer or set NSCLIENT_FLEET_SERVER."
    exit 1
}
if (-not $SyncApiKey) { $SyncApiKey = $env:NSCLIENT_FLEET_SYNC_API_KEY }
if (-not $SyncApiKey -and $env:NSCLIENT_FLEET_API_KEY) {
    Write-Warning "Using NSCLIENT_FLEET_API_KEY for the sync timer. That key ends up on the Icinga VM - a dedicated view_only key (NSCLIENT_FLEET_SYNC_API_KEY) is the better choice."
    $SyncApiKey = $env:NSCLIENT_FLEET_API_KEY
}
if (-not $SyncApiKey) {
    Write-Error "❌ No API key for the sync timer: pass -SyncApiKey or set NSCLIENT_FLEET_SYNC_API_KEY (a view_only key is enough)."
    exit 1
}
# Both are pasted into a double-quoted line of a root-only shell file.
foreach ($value in $FleetServer, $SyncApiKey) {
    if ($value -match '["$`\\\s]') {
        Write-Error "❌ The fleet server url and the API key must not contain quotes, '$', backticks, backslashes or whitespace."
        exit 1
    }
}

# --- the services, as Icinga apply rules -------------------------------------
$catalog = Read-AgentCheckCatalog -Path (Join-Path $scriptDir "agent-checks.json")

# A string literal in the Icinga DSL. '$' starts a macro there, so a literal
# one (NSClient++'s ${keyword}) is doubled.
function ConvertTo-IcingaString {
    param([string]$Value)
    '"' + $Value.Replace('\', '\\').Replace('"', '\"').Replace('$', '$$') + '"'
}
$rules = foreach ($service in $catalog.Services) {
    $lines = @(
        "apply Service $(ConvertTo-IcingaString $service.name) {",
        '  import "fleet-agent-service"',
        '  command_endpoint = host.vars.agent_endpoint',
        "  vars.nscp_query = $(ConvertTo-IcingaString $service.command)"
    )
    $arguments = @($service.arguments | Where-Object { $_ })
    if ($arguments.Count -gt 0) {
        $lines += "  vars.nscp_arguments = [ " + (($arguments | ForEach-Object { ConvertTo-IcingaString "$_" }) -join ", ") + " ]"
    }
    $lines += '  assign where host.vars.fleet_agent'
    $lines += '}'
    $lines -join "`n"
}
$servicesConf = "// Written by setup-icinga-agent-sync.ps1 from agent-checks.json - DO NOT EDIT.`n" + ($rules -join "`n`n")

$syncScript = ((Get-Content -Path (Join-Path $scriptDir "fleet-icinga-agent-sync.sh") -Raw) -replace "`r", "").TrimEnd()

& (Join-Path $parentDir "connect-to-azure.ps1")

if (-not (Get-AzVM -ResourceGroupName $ResourceGroupName -Name $VmName -ErrorAction SilentlyContinue)) {
    Write-Error "❌ '$PwdFile' names VM '$VmName' in resource group '$ResourceGroupName', which no longer exists - run setup-icinga-machine.ps1 first."
    exit 1
}

# --- the payload -------------------------------------------------------------
# Single-quoted here-string + .Replace(), as in setup-icinga-machine.ps1.
Write-Host "Installing the agent templates and the fleet-icinga-agent-sync timer on '$VmName' (polling $FleetServer)..."
$payload = @'
#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
command -v jq >/dev/null 2>&1 || apt-get install -y jq
if id nagios >/dev/null 2>&1; then icinga_user=nagios; else icinga_user=icinga; fi
node_name=$(icinga2 variable get NodeName)

# --- command + templates + services ------------------------------------------
# Names carry a fleet- prefix: "nscp-query", the name the agent documentation
# uses, may already be taken by a hand-written file on this box.
templates=/etc/icinga2/conf.d/fleet-agent-templates.conf
services=/etc/icinga2/conf.d/fleet-agent-services.conf
for f in "$templates" "$services"; do [ ! -f "$f" ] || cp -a "$f" "$f.previous"; done
cat > "$templates" <<'NSCP_ICINGA_EOF'
// Written by setup-icinga-agent-sync.ps1 - DO NOT EDIT.

// Any NSClient++ query, its arguments written as NSClient++ tokens
// (key=value), the same ones check_nrpe -a or the REST API take. The master
// sends the query name and the tokens; the agent's IcingaAgent module runs it.
object CheckCommand "fleet-nscp-query" {
  import "ifw-api"
  vars.ifw_api_command = "$nscp_query$"
  vars.ifw_api_arguments = {
    "(arguments)" = { value = "$nscp_arguments$", skip_key = true }
  }
}

// UP while the agent's zone is connected to this master.
//
// Both templates put their objects in the master's zone by hand. Files under
// conf.d belong to no zone, and Icinga refuses `command_endpoint` on an object
// without one ("Checkable with command endpoint requires a zone").
template Host "fleet-agent-host" {
  zone = ZoneName
  check_command = "cluster-zone"
  max_check_attempts = 2
  check_interval = 30s
  retry_interval = 30s
  vars.fleet_agent = true
}

template Service "fleet-agent-service" {
  zone = ZoneName
  check_command = "fleet-nscp-query"
  max_check_attempts = 1
  check_interval = __INTERVAL__s
  retry_interval = __INTERVAL__s
}
NSCP_ICINGA_EOF
cat > "$services" <<'NSCP_ICINGA_EOF'
__SERVICES_CONF__
NSCP_ICINGA_EOF
chmod 644 "$templates" "$services"

# A file that does not validate would stop the next icinga2 restart, so the
# previous pair goes back (or the new pair goes away) when this one does not.
if ! out=$(icinga2 daemon -C 2>&1); then
    echo "$out" | grep -E 'critical|Error|error' | head -n 20
    for f in "$templates" "$services"; do
        if [ -f "$f.previous" ]; then mv -f "$f.previous" "$f"; else rm -f "$f"; fi
    done
    echo "the agent templates did not validate - left the previous configuration in place"
    exit 1
fi
rm -f "$templates.previous" "$services.previous"
systemctl reload icinga2

# --- the sync timer ----------------------------------------------------------
cat > /usr/local/bin/fleet-icinga-agent-sync <<'NSCP_SYNC_EOF'
__SYNC_SCRIPT__
NSCP_SYNC_EOF
chmod 755 /usr/local/bin/fleet-icinga-agent-sync

cat > /etc/fleet-icinga-agent-sync.conf <<EOF
FLEET_SERVER="__FLEET_SERVER__"
FLEET_API_KEY="__FLEET_API_KEY__"
CURL_OPTS="__CURL_OPTS__"
ICINGA_USER="$icinga_user"
REPLACE_API_HOSTS="__REPLACE__"
ICINGA_API_AUTH="__ADMIN_USER__:__ADMIN_PW__"
EOF
chmod 600 /etc/fleet-icinga-agent-sync.conf

cat > /etc/systemd/system/fleet-icinga-agent-sync.service <<'EOF'
[Unit]
Description=Register NSClient fleet hosts as Icinga 2 agents
After=network-online.target icinga2.service

[Service]
Type=oneshot
ExecStart=/usr/local/bin/fleet-icinga-agent-sync
EOF
cat > /etc/systemd/system/fleet-icinga-agent-sync.timer <<'EOF'
[Unit]
Description=Run fleet-icinga-agent-sync every minute

[Timer]
OnBootSec=1min
OnUnitActiveSec=1min

[Install]
WantedBy=timers.target
EOF
systemctl daemon-reload
systemctl enable --now fleet-icinga-agent-sync.timer

# Run it once, now: a misconfigured key/url must fail this run, not sit
# silently broken in a timer. Through systemctl so it is serialised with a run
# the timer may already have started.
if ! systemctl start fleet-icinga-agent-sync.service; then
    journalctl -u fleet-icinga-agent-sync --no-pager -n 30
    echo "AGENT-SYNC: failed"
    exit 1
fi
journalctl -u fleet-icinga-agent-sync --no-pager -n 12 -o cat

# --- what the agents need to find and trust this master ----------------------
fingerprint=$(openssl x509 -noout -fingerprint -sha256 -in "/var/lib/icinga2/certs/$node_name.crt" | sed 's/^.*=//')
[ -n "$fingerprint" ] || { echo "could not read the fingerprint of /var/lib/icinga2/certs/$node_name.crt"; exit 1; }
echo "AGENT-ENDPOINT: $node_name"
echo "AGENT-FINGERPRINT: $fingerprint"
echo "AGENT-SYNC: ok"
'@
$curlOpts = if ($FleetNoVerify) { "--insecure" } else { "" }
$payload = $payload.Replace('__INTERVAL__', "$($catalog.IntervalSeconds)").Replace('__SERVICES_CONF__', $servicesConf).
Replace('__SYNC_SCRIPT__', $syncScript).Replace('__FLEET_SERVER__', $FleetServer).Replace('__FLEET_API_KEY__', $SyncApiKey).
Replace('__CURL_OPTS__', $curlOpts).Replace('__REPLACE__', $(if ($ReplacePassive) { "1" } else { "0" })).
Replace('__ADMIN_USER__', $existing["Icinga Admin User"]).Replace('__ADMIN_PW__', $existing["Icinga Admin Password"])
# The payload carries this file's own line endings: a core.autocrlf checkout
# would ship \r into bash on the VM.
$payload = $payload -replace "`r", ""

$result = $null
for ($attempt = 1; $attempt -le 3; $attempt++) {
    try {
        $result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName -VMName $VmName `
            -CommandId 'RunShellScript' -ScriptString $payload -ErrorAction Stop
        break
    }
    catch {
        Write-Warning "RunCommand attempt $attempt/3 failed: $($_.Exception.Message)"
        if ($attempt -eq 3) { throw }
        Start-Sleep -Seconds (15 * $attempt)
    }
}
$output = (@($result.Value | ForEach-Object { $_.Message }) -join "`n") -split "`n"
$output | ForEach-Object { Write-Host $_ }
# RunCommand reports "Succeeded" even when the script exits non-zero; the
# marker, printed last, is what says it worked.
if ($result.Status -ne "Succeeded" -or -not ($output -match '^AGENT-SYNC: ok')) {
    Write-Error "❌ The agent setup did not complete on the VM (see the output above) - the fleet server may be unreachable from Azure, or the API key is wrong."
    exit 1
}
$endpoint = ($output | Where-Object { $_ -match '^AGENT-ENDPOINT: (.+)$' } | ForEach-Object { $Matches[1].Trim() } | Select-Object -Last 1)
$fingerprint = ($output | Where-Object { $_ -match '^AGENT-FINGERPRINT: (.+)$' } | ForEach-Object { $Matches[1].Trim() } | Select-Object -Last 1)
if (-not $endpoint -or $fingerprint -notmatch '^([0-9A-Fa-f]{2}:){31}[0-9A-Fa-f]{2}$') {
    Write-Error "❌ The VM did not report its endpoint name and certificate fingerprint."
    exit 1
}

# --- credentials file --------------------------------------------------------
# Replace the lines of an earlier run, keep everything else.
$kept = @(Get-Content -Path $PwdFile | Where-Object { $_ -notmatch '^Icinga (Endpoint|Fingerprint):' })
($kept + @("Icinga Endpoint: $endpoint", "Icinga Fingerprint: $fingerprint")) | Set-Content -Path $PwdFile -Force

Write-Host "✅ Agent registration is running on '$VmName' (every minute); master endpoint '$endpoint'."
Write-Host "● Endpoint name and fingerprint saved to $PwdFile"
Write-Host "Next: point the agents at it:"
Write-Host "  ./build/powershell/icinga/add-icinga-agent-bundle.ps1"
