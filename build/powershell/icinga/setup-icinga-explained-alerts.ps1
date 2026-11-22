<#
.SYNOPSIS
    Set the Icinga VM up for the "Icinga alert emails that explain themselves"
    tutorial: passive services, the notification handler that asks the fleet
    server why, and a working `mail`.
.DESCRIPTION
    The server side of the tutorial, Steps 4 to 6, on the VM that
    setup-icinga-machine.ps1 created. Steps 1 and 2 (a model and a fleet
    server) are taken as given: the flow uses the fleet server the estate is
    enrolled with, which must have explanations switched on (*Settings* in the
    fleet console). The agents are Step 3, configure-explained-agents.ps1, and
    Step 7 is the gate, verify-icinga-explained.ps1.

    On the VM, over RunCommand, it

      1. installs postfix and bsd-mailx, so the handler's `mail` works. Out of
         the box mail is delivered on the VM only, to the mailbox of a local
         user "alerts" (/var/mail/alerts), which is also where a blind copy of
         every mail goes - the gate reads the notification from there, so it
         needs no mail account. -SmtpHost and friends add a relay for mail to
         the outside (Azure blocks outbound port 25, so a submission port and
         an account are needed);
      2. installs check_nsclient to /usr/local/bin and refuses a build without
         `fleet explain`;
      3. writes /etc/icinga2/fleet-explain.env (the fleet url and an API key,
         readable by the Icinga daemon user only) and
         /etc/icinga2/scripts/mail-service-explained.sh - the tutorial's
         handler, unchanged (icinga/explained/mail-service-explained.sh);
      4. writes conf.d/nscp-passive.conf - an API user that may submit results
         and nothing else, a passive service per entry of
         explained-checks.json and one Host per machine of the estate manifest
         - and conf.d/notify-explained.conf, the tutorial's NotificationCommand
         and apply rule;
      5. validates the configuration, reloads Icinga and prints its CA
         certificate and node name, which the agents need to verify the API
         (`verify mode = peer-cert`, as the tutorial has it): the certificate
         is issued by that CA, for that name.

    The CA is saved next to the credentials file as .icinga-ca.crt, and the API
    user is added to that file. Re-running is safe: every file is rewritten,
    and a new password for the API user is only generated when there is none.

    Where the configuration differs from the tutorial is explained at the top
    of icinga/explained/nscp-passive.conf.
.PARAMETER FleetServer
    Fleet server url the handler asks. Defaults to $env:NSCLIENT_FLEET_SERVER,
    then the "Fleet Server" line of the credentials file.
.PARAMETER ExplainApiKey
    Fleet API key for the handler. It is stored on the Icinga VM, and any role
    may ask for explanations, so use a view_only key. Defaults to
    $env:NSCLIENT_FLEET_SYNC_API_KEY, then (with a warning) to
    $env:NSCLIENT_FLEET_API_KEY.
.PARAMETER RemoveKey
    Blank the API key on the VM and stop. Notifications are still mailed,
    with "No explanation available" in place of the explanation.
.PARAMETER PwdFile
    Credentials file of the Icinga VM (default: ../.icinga.pwd).
.PARAMETER ManifestFile
    The estate manifest whose machines become Icinga hosts (default:
    ../.fleet-machines.json).
.PARAMETER ReplaceHosts
    Take over a host of the same name that another flow defined: the agent-mode
    sync's conf.d/fleet-agents/<host>.conf is removed, and a host the turn-key
    passive flow created through the API is deleted. Without it the script
    stops when it meets one.
.PARAMETER CheckNsclientUrl
    Download check_nsclient (linux x64) from this url. Default: the latest
    release on GitHub - unless the VM already has a check_nsclient with
    `fleet explain`, which is then kept. `fleet explain` is newer than
    check_nsclient 1.1.3.
.PARAMETER ExplainSettle
    vars.explain_settle on every host: the seconds the handler waits before
    asking, so the agent's failure report is there first (the tutorial's
    "Timing" section). Default 75; keep it at or below 90, since the handler
    may then wait another 150 s and the command times out after 4 minutes.
.PARAMETER MaxCheckAttempts
    max_check_attempts of the passive services (default 1: the first failed
    result is a hard problem and notifies).
.PARAMETER MailTo
    Address of the notified user "icingaadmin". Default: alerts@localhost, the
    mailbox on the VM. An address elsewhere needs -SmtpHost.
.PARAMETER MailFrom
    Rewrite the sender of every mail to this address. Most relays refuse or
    rewrite a sender they do not know, and the VM's own is
    nagios@<its hostname>.
.PARAMETER SmtpHost
    Relay outbound mail through this server.
.PARAMETER SmtpPort
    Its port (default 587, STARTTLS; 465 switches to implicit TLS).
.PARAMETER SmtpUser
    Account for the relay. The password comes from -SmtpPassword or
    $env:NSCLIENT_SMTP_PASSWORD.
.EXAMPLE
    # Everything local: notifications land in /var/mail/alerts on the VM.
    ./setup-icinga-explained-alerts.ps1 -ReplaceHosts
.EXAMPLE
    # Real mail, through a relay:
    $env:NSCLIENT_SMTP_PASSWORD = "..."
    ./setup-icinga-explained-alerts.ps1 -MailTo you@example.com -MailFrom alerts@example.com `
        -SmtpHost smtp.example.com -SmtpUser alerts@example.com
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ExplainApiKey = "",
    [switch]$RemoveKey,
    [string]$PwdFile = "",
    [string]$ManifestFile = "",
    [switch]$ReplaceHosts,
    [string]$CheckNsclientUrl = "",
    [ValidateRange(0, 90)]
    [int]$ExplainSettle = 75,
    [ValidateRange(1, 10)]
    [int]$MaxCheckAttempts = 1,
    [string]$MailTo = "alerts@localhost",
    [string]$MailFrom = "",
    [string]$SmtpHost = "",
    [ValidateRange(1, 65535)]
    [int]$SmtpPort = 587,
    [string]$SmtpUser = "",
    [string]$SmtpPassword = ""
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
foreach ($key in "VM Name", "Resource Group", "Icinga Admin User", "Icinga Admin Password") {
    if (-not $existing[$key]) {
        Write-Error "❌ '$PwdFile' has no '$key' line - it was not written by setup-icinga-machine.ps1."
        exit 1
    }
}
$VmName = $existing["VM Name"]
$ResourceGroupName = $existing["Resource Group"]

function Invoke-IcingaVmScript {
    param([Parameter(Mandatory)][string]$Payload)
    # The payload carries this file's own line endings: a core.autocrlf
    # checkout would ship \r into bash on the VM.
    $Payload = $Payload -replace "`r", ""
    $result = $null
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        try {
            $result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName -VMName $VmName `
                -CommandId 'RunShellScript' -ScriptString $Payload -ErrorAction Stop
            break
        }
        catch {
            Write-Warning "RunCommand attempt $attempt/3 failed: $($_.Exception.Message)"
            if ($attempt -eq 3) { throw }
            Start-Sleep -Seconds (15 * $attempt)
        }
    }
    return , @((@($result.Value | ForEach-Object { $_.Message }) -join "`n") -split "`n")
}

if ($RemoveKey) {
    Write-Host "Removing the fleet API key from '$VmName'..."
    $output = Invoke-IcingaVmScript -Payload @'
#!/bin/bash
f=/etc/icinga2/fleet-explain.env
[ ! -f "$f" ] || sed -i 's/^FLEET_API_KEY=.*/FLEET_API_KEY=/' "$f"
# RunCommand keeps the scripts it ran, and an earlier one carried the key.
# The pattern is split so that this script, which RunCommand also keeps, does
# not match itself.
for s in $(grep -rl 'ns''k_' /var/lib/waagent/run-command/download 2>/dev/null); do : > "$s"; done
left=$(grep -rl 'ns''k_' /etc/icinga2 /var/lib/waagent/run-command 2>/dev/null | wc -l)
[ "$left" = 0 ] && echo "EXPLAINED-KEY: removed" || echo "EXPLAINED-KEY: $left file(s) still hold a key"
'@
    $output | ForEach-Object { Write-Host $_ }
    if (-not ($output -match '^EXPLAINED-KEY: removed')) {
        Write-Error "❌ The key was not removed from the VM (see the output above)."
        exit 1
    }
    Write-Host "✅ No fleet API key is left on '$VmName'. Notifications are mailed without an explanation until this script is run again."
    return
}

if (-not $FleetServer) { $FleetServer = $existing["Fleet Server"] }
if (-not $FleetServer) {
    Write-Error "❌ No fleet server: pass -FleetServer or set NSCLIENT_FLEET_SERVER."
    exit 1
}
$FleetServer = $FleetServer.TrimEnd('/')
if (-not $ExplainApiKey) { $ExplainApiKey = $env:NSCLIENT_FLEET_SYNC_API_KEY }
if (-not $ExplainApiKey -and $env:NSCLIENT_FLEET_API_KEY) {
    Write-Warning "Using NSCLIENT_FLEET_API_KEY for the notification handler. That key ends up on the Icinga VM - a dedicated view_only key (NSCLIENT_FLEET_SYNC_API_KEY) is the better choice. -RemoveKey takes it off the VM again."
    $ExplainApiKey = $env:NSCLIENT_FLEET_API_KEY
}
if (-not $ExplainApiKey) {
    Write-Error "❌ No API key for the handler: pass -ExplainApiKey or set NSCLIENT_FLEET_SYNC_API_KEY (a view_only key is enough)."
    exit 1
}
if (-not $SmtpPassword) { $SmtpPassword = "$env:NSCLIENT_SMTP_PASSWORD" }

# Everything below is pasted into a root shell script on the VM.
foreach ($value in $FleetServer, $ExplainApiKey, $CheckNsclientUrl) {
    if ($value -match '["''$`\\\s]') {
        Write-Error "❌ The fleet server url, the API key and -CheckNsclientUrl must not contain quotes, '$', backticks, backslashes or whitespace."
        exit 1
    }
}
foreach ($address in $MailTo, $MailFrom) {
    if ($address -and $address -notmatch '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+$') {
        Write-Error "❌ '$address' is not a plain mail address."
        exit 1
    }
}
if ($SmtpHost -and $SmtpHost -notmatch '^[A-Za-z0-9.-]+$') {
    Write-Error "❌ -SmtpHost must be a host name or an address."
    exit 1
}
if ($SmtpUser -match '[\s:''"]' -or $SmtpPassword -match "[`r`n]") {
    Write-Error "❌ -SmtpUser must not contain whitespace, quotes or ':', and the password no line break."
    exit 1
}
if ($SmtpUser -and -not $SmtpPassword) {
    Write-Error "❌ -SmtpUser needs a password: pass -SmtpPassword or set NSCLIENT_SMTP_PASSWORD."
    exit 1
}
if (($SmtpUser -or $MailTo -notmatch '@localhost$') -and -not $SmtpHost) {
    Write-Error "❌ Mail to '$MailTo' has to leave the VM, and Azure blocks outbound port 25: pass -SmtpHost (and usually -SmtpUser, -MailFrom)."
    exit 1
}

# --- the hosts ---------------------------------------------------------------
if (-not $ManifestFile) { $ManifestFile = Join-Path $parentDir ".fleet-machines.json" }
$machines = @(Read-FleetManifest -Path $ManifestFile)
if ($machines.Count -eq 0) {
    Write-Error "❌ No machines in '$ManifestFile' - provision the estate first (provision-fleet-machines.ps1)."
    exit 1
}
# The agents submit as ${host_lc}; the fleet knows them by the same name, so
# the handler's --host needs no vars.fleet_host.
$hostNames = @($machines | ForEach-Object { "$($_.Name)".ToLowerInvariant() })
foreach ($name in $hostNames) {
    if ($name -notmatch '^[a-z0-9][a-z0-9.-]*$') {
        Write-Error "❌ Machine name '$name' is not a plain host name."
        exit 1
    }
}

# --- the Icinga configuration ------------------------------------------------
$catalog = Read-ExplainedCheckCatalog -Path (Join-Path $scriptDir "explained-checks.json")
$apiUser = "nscp-explained"
$apiPassword = $existing["Explained Agent Password"]
if (-not $apiPassword) {
    $apiPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 24 | ForEach-Object { [char]$_ })
}
$passiveConf = (Get-Content -Path (Join-Path $scriptDir "explained/nscp-passive.conf") -Raw).
Replace('__API_USER__', $apiUser).Replace('__API_PASSWORD__', $apiPassword).
Replace('__ATTEMPTS__', "$MaxCheckAttempts").Replace('__FRESHNESS__', "$(2 * $catalog.IntervalSeconds)").
Replace('__SERVICES__', (($catalog.ServiceNames | ForEach-Object { '"' + $_ + '"' }) -join ", ")).
Replace('__SETTLE__', "$ExplainSettle").Replace('__MAIL_TO__', $MailTo)
$passiveConf += "`n" + (($hostNames | ForEach-Object { "object Host `"$_`" {`n  import `"nscp-passive-host`"`n}" }) -join "`n`n") + "`n"
$notifyConf = Get-Content -Path (Join-Path $scriptDir "explained/notify-explained.conf") -Raw
$handler = Get-Content -Path (Join-Path $scriptDir "explained/mail-service-explained.sh") -Raw

$payload = @'
#!/bin/bash
# Kept quiet on purpose: RunCommand returns only the last 4 KB of output, and
# the CA certificate and the markers at the end have to fit in it.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
fail() { echo "EXPLAINED-SETUP-FAILED: $*"; exit 1; }

icinga_user=nagios
id "$icinga_user" >/dev/null 2>&1 || icinga_user=icinga
api='https://localhost:5665/v1'
admin='__ADMIN_USER__:__ADMIN_PW__'

# --- mail --------------------------------------------------------------------
if ! command -v postfix >/dev/null 2>&1 || ! command -v mail >/dev/null 2>&1; then
    echo "postfix postfix/main_mailer_type select Local only" | debconf-set-selections
    echo "postfix postfix/mailname string $(hostname -f 2>/dev/null || hostname)" | debconf-set-selections
    apt-get update -qq >/dev/null 2>&1 || true
    apt-get install -y -qq postfix bsd-mailx libsasl2-modules curl ca-certificates >/dev/null 2>&1 ||
        fail "could not install postfix and bsd-mailx"
fi
# The local mailbox: every mail is copied here, whoever it is addressed to.
id alerts >/dev/null 2>&1 || useradd --system --create-home --shell /usr/sbin/nologin alerts
postconf -e 'inet_interfaces = loopback-only' 'always_bcc = alerts@localhost' \
    'mydestination = $myhostname, localhost.$mydomain, localhost'

smtp_host='__SMTP_HOST__'
smtp_port='__SMTP_PORT__'
if [ -n "$smtp_host" ]; then
    postconf -e "relayhost = [$smtp_host]:$smtp_port" 'default_transport = smtp' 'relay_transport = relay' \
        'smtp_tls_security_level = encrypt' 'smtp_tls_CAfile = /etc/ssl/certs/ca-certificates.crt'
    if [ "$smtp_port" = 465 ]; then postconf -e 'smtp_tls_wrappermode = yes'; else postconf -e 'smtp_tls_wrappermode = no'; fi
    if [ -n '__SMTP_USER__' ]; then
        ( umask 077; cat > /etc/postfix/sasl_passwd <<'EOF_SASL'
[__SMTP_HOST__]:__SMTP_PORT__ __SMTP_USER__:__SMTP_PASSWORD__
EOF_SASL
        )
        postmap /etc/postfix/sasl_passwd
        postconf -e 'smtp_sasl_auth_enable = yes' 'smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd' \
            'smtp_sasl_security_options = noanonymous'
    else
        rm -f /etc/postfix/sasl_passwd /etc/postfix/sasl_passwd.db
        postconf -e 'smtp_sasl_auth_enable = no'
    fi
else
    # No relay: mail stays on the VM, and mail for elsewhere is refused at once
    # instead of queueing for a port 25 that Azure does not let out.
    rm -f /etc/postfix/sasl_passwd /etc/postfix/sasl_passwd.db
    postconf -e 'relayhost =' 'default_transport = error' 'relay_transport = error' 'smtp_sasl_auth_enable = no'
fi
if [ -n '__MAIL_FROM__' ]; then
    echo '/.+/ __MAIL_FROM__' > /etc/postfix/sender_canonical
    postconf -e 'sender_canonical_classes = envelope_sender, header_sender' \
        'sender_canonical_maps = regexp:/etc/postfix/sender_canonical'
else
    postconf -e 'sender_canonical_maps ='
fi
systemctl enable postfix >/dev/null 2>&1 || true
systemctl restart postfix

# A mail has to arrive in the mailbox, or the gate has nothing to read.
stamp="explained-alerts setup $(date +%s)"
echo "Sent by setup-icinga-explained-alerts.ps1." | mail -s "$stamp" alerts@localhost
for i in $(seq 1 20); do
    grep -qF "Subject: $stamp" /var/mail/alerts 2>/dev/null && break
    sleep 1
done
grep -qF "Subject: $stamp" /var/mail/alerts 2>/dev/null ||
    fail "a test mail did not reach /var/mail/alerts: $(postqueue -p 2>&1 | tail -n 3 | tr '\n' ' ')"
echo "MAIL-LOCAL: ok"
if [ '__MAIL_TO__' != 'alerts@localhost' ]; then
    echo "Sent by setup-icinga-explained-alerts.ps1 to check the relay." | mail -s "$stamp (relay test)" '__MAIL_TO__'
    sleep 20
    if postqueue -p 2>/dev/null | grep -q '^Mail queue is empty'; then
        echo "MAIL-RELAY: the relay accepted a test mail for __MAIL_TO__"
    else
        echo "MAIL-RELAY: NOT DELIVERED - $(postqueue -p 2>&1 | grep -v '^-' | tail -n 4 | tr '\n' ' ' | cut -c1-400)"
    fi
fi

# --- check_nsclient ----------------------------------------------------------
bin=/usr/local/bin/check_nsclient
explains() { "$1" fleet explain --help >/dev/null 2>&1; }
url='__CHECK_NSCLIENT_URL__'
if [ -n "$url" ] || ! { [ -x "$bin" ] && explains "$bin"; }; then
    if [ -z "$url" ]; then
        case "$(uname -m)" in aarch64) arch=arm64 ;; *) arch=x64 ;; esac
        url=$(curl -fsSL https://api.github.com/repos/mickem/check_nsclient/releases/latest |
            grep -o '"browser_download_url": *"[^"]*"' | cut -d'"' -f4 | grep -- "-linux-$arch\$" | head -n 1) || true
        [ -n "$url" ] || fail "could not find the latest check_nsclient release on GitHub - pass -CheckNsclientUrl"
    fi
    curl -fsSL -o "$bin.new" "$url" || fail "could not download check_nsclient"
    chmod 755 "$bin.new"
    if ! explains "$bin.new"; then
        version=$("$bin.new" --version 2>&1 | head -n 1) || true
        rm -f "$bin.new"
        fail "that check_nsclient ($version) has no 'fleet explain' - it is newer than 1.1.3; pass -CheckNsclientUrl with a build that has it"
    fi
    mv -f "$bin.new" "$bin"
fi
echo "CHECK-NSCLIENT: $("$bin" --version 2>&1 | head -n 1)"

# --- the handler and its credentials -----------------------------------------
( umask 077; cat > /etc/icinga2/fleet-explain.env <<'EOF_ENV'
FLEET_URL=__FLEET_SERVER__
FLEET_API_KEY=__FLEET_API_KEY__
EOF_ENV
)
chown "root:$icinga_user" /etc/icinga2/fleet-explain.env
chmod 640 /etc/icinga2/fleet-explain.env

install -d /etc/icinga2/scripts
cat > /etc/icinga2/scripts/mail-service-explained.sh <<'EOF_HANDLER'
__HANDLER__
EOF_HANDLER
chmod 755 /etc/icinga2/scripts/mail-service-explained.sh

# The key has to work as the daemon user, without a profile and against the
# system trust store. A host that does not exist costs no model call: the
# fleet server answers 404 before it asks anyone.
probe=$(runuser -u "$icinga_user" -- bash -c '. /etc/icinga2/fleet-explain.env; export FLEET_URL FLEET_API_KEY
    echo probe | timeout 60 /usr/local/bin/check_nsclient fleet --timeout-s 30 explain --host no-such-host-explained-setup 2>&1' || true)
case "$probe" in
    *404*|*"o such host"*|*"ot found"*) echo "FLEET-KEY: ok" ;;
    *) fail "the handler cannot ask __FLEET_SERVER__ as $icinga_user: $(echo "$probe" | tr '\n' ' ' | cut -c1-300)" ;;
esac

# --- hosts another flow already defined --------------------------------------
replace='__REPLACE__'
for host in __HOSTS__; do
    agent_conf="/etc/icinga2/conf.d/fleet-agents/$host.conf"
    if [ -f "$agent_conf" ]; then
        [ "$replace" = 1 ] || fail "$host is an agent-mode host ($agent_conf) - re-run with -ReplaceHosts to take it over"
        rm -f "$agent_conf"
    fi
    package=$(curl -ks -u "$admin" -H 'Accept: application/json' "$api/objects/hosts/$host" |
        grep -o '"package":"[^"]*"' | head -n 1 | cut -d'"' -f4) || true
    if [ "$package" = "_api" ]; then
        [ "$replace" = 1 ] || fail "$host was created through the API (the turn-key passive flow) - re-run with -ReplaceHosts to take it over"
        curl -ks -o /dev/null -u "$admin" -X DELETE -H 'Accept: application/json' "$api/objects/hosts/$host?cascade=1" || true
    fi
done

# --- the Icinga configuration ------------------------------------------------
conf=/etc/icinga2/conf.d
for f in nscp-passive notify-explained; do
    [ ! -f "$conf/$f.conf" ] || cp -p "$conf/$f.conf" "/tmp/$f.conf.before"
done
( umask 027; cat > "$conf/nscp-passive.conf" <<'EOF_PASSIVE'
__PASSIVE_CONF__
EOF_PASSIVE
)
chown "root:$icinga_user" "$conf/nscp-passive.conf"
chmod 640 "$conf/nscp-passive.conf"
cat > "$conf/notify-explained.conf" <<'EOF_NOTIFY'
__NOTIFY_CONF__
EOF_NOTIFY

if ! icinga2 daemon -C > /tmp/explained-validate.log 2>&1; then
    grep -v 'information/' /tmp/explained-validate.log | cut -c1-300 | head -n 12
    # Leave the daemon's next reload something it can load.
    for f in nscp-passive notify-explained; do
        if [ -f "/tmp/$f.conf.before" ]; then cp -p "/tmp/$f.conf.before" "$conf/$f.conf"; else rm -f "$conf/$f.conf"; fi
    done
    fail "icinga2 refused the configuration (the files were put back as they were)"
fi
systemctl reload icinga2
first=$(set -- __HOSTS__; echo "$1")
for i in $(seq 1 30); do
    code=$(curl -ks -o /dev/null -w '%{http_code}' -u "$admin" -H 'Accept: application/json' "$api/objects/services/$first!__FAULT__" || true)
    [ "$code" = 200 ] && break
    sleep 2
done
[ "$code" = 200 ] || fail "icinga2 did not come back with the service $first!__FAULT__ (HTTP $code)"

# The name the API's certificate is issued for: the agents verify it, so they
# have to address the VM by it.
echo "ICINGA-NODE: $(icinga2 variable get NodeName 2>/dev/null | head -n 1)"
echo "ICINGA-CA-BEGIN"
cat /var/lib/icinga2/certs/ca.crt
echo "ICINGA-CA-END"
echo "EXPLAINED-SETUP: ok"
'@
$payload = $payload.Replace('__HANDLER__', $handler.TrimEnd()).Replace('__PASSIVE_CONF__', $passiveConf.TrimEnd()).
Replace('__NOTIFY_CONF__', $notifyConf.TrimEnd()).
Replace('__ADMIN_USER__', $existing["Icinga Admin User"]).Replace('__ADMIN_PW__', $existing["Icinga Admin Password"]).
Replace('__SMTP_HOST__', $SmtpHost).Replace('__SMTP_PORT__', "$SmtpPort").Replace('__SMTP_USER__', $SmtpUser).
Replace('__SMTP_PASSWORD__', $SmtpPassword).Replace('__MAIL_FROM__', $MailFrom).Replace('__MAIL_TO__', $MailTo).
Replace('__CHECK_NSCLIENT_URL__', $CheckNsclientUrl).Replace('__FLEET_SERVER__', $FleetServer).
Replace('__FLEET_API_KEY__', $ExplainApiKey).Replace('__REPLACE__', $(if ($ReplaceHosts) { "1" } else { "0" })).
Replace('__HOSTS__', ($hostNames -join " ")).Replace('__FAULT__', $catalog.Fault)

Write-Host "● Connecting to Azure account..."
if (-not (Get-AzContext)) { Connect-AzAccount | Out-Null }
Write-Host "Installing mail, check_nsclient, the handler and $($hostNames.Count) passive host(s) on '$VmName' (asking $FleetServer)..."
$output = Invoke-IcingaVmScript -Payload $payload

$inCa = $false
$caLines = @()
foreach ($line in $output) {
    if ($line -match '^ICINGA-CA-BEGIN') { $inCa = $true; continue }
    if ($line -match '^ICINGA-CA-END') { $inCa = $false; continue }
    if ($inCa) { $caLines += $line.TrimEnd(); continue }
    Write-Host $line
}
# RunCommand reports "Succeeded" even when the script exits non-zero; the
# marker, printed last, is what says it worked.
if (-not ($output -match '^EXPLAINED-SETUP: ok')) {
    Write-Error "❌ The setup did not complete on the VM (see the output above)."
    exit 1
}
if (-not ($caLines -match 'BEGIN CERTIFICATE') -or -not ($caLines -match 'END CERTIFICATE')) {
    Write-Error "❌ The VM did not hand back its CA certificate."
    exit 1
}
$nodeName = ($output | Where-Object { $_ -match '^ICINGA-NODE: (\S+)\s*$' } | ForEach-Object { $Matches[1] } | Select-Object -Last 1)
if ("$nodeName" -notmatch '^[A-Za-z0-9][A-Za-z0-9.-]*$') {
    Write-Error "❌ The VM did not report its Icinga node name."
    exit 1
}
$caFile = Join-Path (Split-Path -Parent $PwdFile) ".icinga-ca.crt"
Set-Content -Path $caFile -Value $caLines -Force

# --- credentials file --------------------------------------------------------
# Replace the lines of an earlier run, keep everything else.
$kept = @(Get-Content -Path $PwdFile | Where-Object { $_ -notmatch '^(Explained Agent (User|Password)|Icinga Node Name|Alert Mailbox|Alert Mail To):' })
($kept + @(
    "Explained Agent User: $apiUser",
    "Explained Agent Password: $apiPassword",
    "Icinga Node Name: $nodeName",
    "Alert Mailbox: /var/mail/alerts",
    "Alert Mail To: $MailTo"
)) | Set-Content -Path $PwdFile -Force

Write-Host "✅ '$VmName' mails explained notifications for $($hostNames -join ', ') to $MailTo (a copy of every mail: /var/mail/alerts on the VM)."
Write-Host "● Icinga CA saved to $caFile, API user '$apiUser' to $PwdFile"
Write-Host "Next: configure the agents, then run the gate:"
Write-Host "  ./build/powershell/icinga/configure-explained-agents.ps1"
Write-Host "  ./build/powershell/icinga/verify-icinga-explained.ps1"
