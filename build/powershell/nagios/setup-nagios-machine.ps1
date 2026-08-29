#Requires -Module Az.Accounts
#Requires -Module Az.Compute
#Requires -Module Az.Network

<#
.SYNOPSIS
    Provision an Ubuntu VM in Azure running Nagios Core 4 with NRDP, wired to an
    NSClient fleet server so enrolled hosts register themselves in Nagios.
.DESCRIPTION
    Stage 1 of the turn-key monitoring flow (see README.md). Creates the VM,
    installs Ubuntu's nagios4 + Apache and the NRDP passive-result receiver
    (random token), and - when -FleetServer is given - installs
    fleet-nagios-sync: a one-minute systemd timer that polls GET /api/hosts on
    the fleet server and mirrors every enrolled host into Nagios as a passive
    host with one service per entry in passive-checks.json. Hosts removed from
    the fleet disappear from Nagios on the next sync.

    The passive service objects use freshness checking: a host that stops
    submitting results goes CRITICAL ("stale") after three intervals.

    Everything a later stage needs (Nagios UI password, NRDP url + token) is
    written to .nagios.pwd next to the other scripts' .vm.pwd files.

    NRDP listens on plain HTTP here, so the token travels in clear - fine for a
    throwaway test estate, not for anything else.
.PARAMETER ResourceGroupName
    The name for the new resource group.
.PARAMETER Location
    The Azure region where resources will be deployed.
.PARAMETER VmName
    The name for the new virtual machine.
.PARAMETER UbuntuVersion
    The Ubuntu version to deploy ("24.04" or "22.04"; both package nagios4).
.PARAMETER AdminUsername
    The administrator username for the new VM.
.PARAMETER FleetServer
    The NSClient fleet server whose hosts should be mirrored into Nagios, e.g.
    "https://fleet.example.com". Omit to get a plain Nagios+NRDP box without
    host auto-registration.
.PARAMETER SyncApiKey
    Fleet API key ("nsk_...") the sync timer polls GET /api/hosts with. It only
    needs to read, so mint it for a view_only user - the key lives on the Nagios
    VM. Defaults to $env:NSCLIENT_FLEET_SYNC_API_KEY, then (with a warning) to
    $env:NSCLIENT_FLEET_API_KEY.
.PARAMETER FleetInsecure
    Allow a plain http:// fleet server url.
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate on the VM (self-signed
    test server without -FleetCaFile). Implies -FleetInsecure.
.PARAMETER FleetCaFile
    Local PEM file with the CA that issued the fleet server certificate; copied
    to the VM and used by the sync timer's curl.
.PARAMETER NrdpVersion
    NRDP release to install (a NagiosEnterprises/nrdp tag).
.PARAMETER VmSize
    Azure VM size. The default is a 1-vCPU size: a subscription's vCPU quota is
    what caps the size of an estate, so halving the vCPUs per machine doubles
    how many fit. Must be a Generation 2 size (every image used here is Gen2).
.PARAMETER PwdFile
    Where to write the credentials file (default: build/powershell/.nagios.pwd).
.EXAMPLE
    $env:NSCLIENT_FLEET_SYNC_API_KEY = "nsk_..."   # a view_only key
    ./setup-nagios-machine.ps1 -FleetServer https://fleet.example.com
#>
param(
    [string]$ResourceGroupName = "NSCP-Nagios-RG",
    [string]$Location = "WestEurope",
    [string]$VmName = "NSCP-Nagios",
    [string]$UbuntuVersion = "24.04",
    [string]$AdminUsername = "azureadmin",
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$SyncApiKey = "",
    [switch]$FleetInsecure,
    [switch]$FleetNoVerify,
    [string]$FleetCaFile = "",
    [string]$NrdpVersion = "2.0.6",
    [string]$VmSize = "Standard_F1as_v7",
    [string]$PwdFile = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")

# --- parameter validation, before anything costs money -----------------------
if ($FleetServer) {
    if (-not $SyncApiKey) { $SyncApiKey = $env:NSCLIENT_FLEET_SYNC_API_KEY }
    if (-not $SyncApiKey -and $env:NSCLIENT_FLEET_API_KEY) {
        Write-Warning "Using NSCLIENT_FLEET_API_KEY for the sync timer. That key ends up on the Nagios VM - a dedicated view_only key (NSCLIENT_FLEET_SYNC_API_KEY) is the better choice."
        $SyncApiKey = $env:NSCLIENT_FLEET_API_KEY
    }
    if (-not $SyncApiKey) {
        Write-Error "❌ -FleetServer needs an API key for the sync timer: pass -SyncApiKey or set NSCLIENT_FLEET_SYNC_API_KEY (a view_only key is enough)."
        exit 1
    }
    if ($FleetServer -notmatch '^https://' -and -not ($FleetInsecure -or $FleetNoVerify)) {
        Write-Error "❌ '$FleetServer' is not https. Pass -FleetInsecure if this really is a plain-http test server."
        exit 1
    }
    if ($FleetCaFile -and -not (Test-Path $FleetCaFile)) {
        Write-Error "❌ -FleetCaFile '$FleetCaFile' does not exist."
        exit 1
    }
    Write-Host "Checking the fleet server at $FleetServer..."
    Test-FleetServer -FleetServer $FleetServer -SkipCertificateCheck:$FleetNoVerify | Out-Null
    Write-Host "✅ Fleet server is answering. (The VM must be able to reach the same url from Azure.)"
}
elseif ($SyncApiKey) {
    Write-Error "❌ -SyncApiKey needs -FleetServer."
    exit 1
}

# The catalog is the single source for the service names and the interval:
# the bundle submits every <interval>, so a result older than three of them
# is stale. Note the VM freezes the catalog at provisioning time - re-run this
# script after editing passive-checks.json.
$catalog = Read-PassiveCheckCatalog -Path (Join-Path $scriptDir "passive-checks.json")
$serviceNames = @($catalog.Services | ForEach-Object { $_.name })
$freshnessThreshold = 3 * $catalog.IntervalSeconds

$PublisherName = "Canonical"
switch ($UbuntuVersion) {
    "24.04" {
        $Offer = "ubuntu-24_04-lts"
        $Skus = "server"
    }
    "22.04" {
        $Offer = "0001-com-ubuntu-server-jammy"
        $Skus = "22_04-lts-gen2"
    }
    default {
        Write-Error "❌ Unsupported Ubuntu version: $UbuntuVersion. Supported versions: 24.04, 22.04"
        exit 1
    }
}

$syncScriptFile = Join-Path $scriptDir "fleet-nagios-sync.sh"
# CRLF from a Windows checkout would put a \r in the shebang; normalize here so
# the heredoc below ships clean LF content whatever git's autocrlf did.
$syncScript = (Get-Content -Path $syncScriptFile -Raw) -replace "`r", ""

& (Join-Path $parentDir "connect-to-azure.ps1")

# Run a script on the VM via RunCommand, retrying transient Azure API errors
# (same helper as the other setup scripts; Azure serialises RunCommands per VM).
function Invoke-VMRun {
    param(
        [Parameter(Mandatory)][string]$ResourceGroupName,
        [Parameter(Mandatory)][string]$VMName,
        [Parameter(Mandatory)][string]$ScriptString,
        [int]$Retries = 3
    )
    for ($attempt = 1; $attempt -le $Retries; $attempt++) {
        try {
            return Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName -VMName $VMName `
                -CommandId 'RunShellScript' -ScriptString $ScriptString -ErrorAction Stop
        }
        catch {
            Write-Warning "RunCommand attempt $attempt/$Retries failed: $($_.Exception.Message)"
            if ($attempt -eq $Retries) { throw }
            Start-Sleep -Seconds (15 * $attempt)
        }
    }
}

# Run a payload and require a success marker in its output: RunCommand reports
# "Succeeded" even when the script exits non-zero, so the marker - not the
# status - is what says the step actually worked.
function Invoke-VMStep {
    param(
        [Parameter(Mandatory)][string]$ScriptString,
        [Parameter(Mandatory)][string]$Marker,
        [Parameter(Mandatory)][string]$FailureMessage
    )
    # The payloads carry this .ps1 file's own line endings: a core.autocrlf
    # checkout would ship \r into bash on the VM, breaking every line.
    $ScriptString = $ScriptString -replace "`r", ""
    $result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $ScriptString
    $messages = @($result.Value | ForEach-Object { $_.Message })
    $messages | ForEach-Object { $_ }
    if ($result.Status -ne "Succeeded" -or -not ($messages -match [regex]::Escape($Marker))) {
        Write-Error "❌ $FailureMessage"
        exit 1
    }
}

# --- SSH key (same conventions as the other setup scripts) -------------------
$sshDir = Join-Path $HOME ".ssh"
if (-not (Test-Path $sshDir)) { New-Item -ItemType Directory -Path $sshDir -Force | Out-Null }
$sshKeyPath = Join-Path $sshDir "az_$($VmName)_rsa"
if (-not (Test-Path $sshKeyPath)) {
    Write-Host "Generating SSH key pair..."
    ssh-keygen -t rsa -b 4096 -f $sshKeyPath -N '' -q
}
$sshPublicKey = Get-Content "$sshKeyPath.pub"

# --- Azure resources ---------------------------------------------------------
Write-Host "Creating resource group: $ResourceGroupName..."
New-AzResourceGroup -Name $ResourceGroupName -Location $Location -Force

Write-Host "Configuring virtual network and security rules..."
$subnetConfig = New-AzVirtualNetworkSubnetConfig -Name "default-subnet" -AddressPrefix "10.0.0.0/24"
$vnet = New-AzVirtualNetwork -Name "$($VmName)-vnet" -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $subnetConfig
$publicIp = New-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName -Location $Location -AllocationMethod Static -Sku Standard

# 22 for SSH, 80 for both the Nagios UI (/nagios4) and NRDP (/nrdp) - the
# agents submit their passive results to the latter.
$nsgRuleSSH = New-AzNetworkSecurityRuleConfig -Name "Allow-SSH" -Protocol Tcp -Direction Inbound -Priority 1000 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "22" -Access Allow
$nsgRuleHTTP = New-AzNetworkSecurityRuleConfig -Name "Allow-HTTP" -Protocol Tcp -Direction Inbound -Priority 1010 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "80" -Access Allow
$nsg = New-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName -Location $Location -SecurityRules $nsgRuleSSH, $nsgRuleHTTP

$nic = New-AzNetworkInterface -Name "$($VmName)-nic" -ResourceGroupName $ResourceGroupName -Location $Location -SubnetId $vnet.Subnets[0].Id -PublicIpAddressId $publicIp.Id -NetworkSecurityGroupId $nsg.Id

Write-Host "Creating the Virtual Machine: $VmName with Ubuntu $UbuntuVersion..."
$vmConfig = New-AzVMConfig -VMName $VmName -VMSize $VMSize | `
    Set-AzVMOperatingSystem -Linux -ComputerName $VmName -Credential (New-Object PSCredential($AdminUsername, (ConvertTo-SecureString -String "TempPassword123!" -AsPlainText -Force))) -DisablePasswordAuthentication | `
    Set-AzVMSourceImage -PublisherName $PublisherName -Offer $Offer -Skus $Skus -Version "latest" | `
    Add-AzVMNetworkInterface -Id $nic.Id | `
    Add-AzVMSshPublicKey -KeyData $sshPublicKey -Path "/home/$AdminUsername/.ssh/authorized_keys"

New-AzVM -ResourceGroupName $ResourceGroupName -Location $Location -VM $vmConfig

$vmPublicIp = (Get-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName).IpAddress
Write-Host "Connect via SSH: ssh -i $sshKeyPath $AdminUsername@$vmPublicIp"

# --- secrets -----------------------------------------------------------------
$NagiosPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 16 | ForEach-Object { [char]$_ })
$NrdpToken = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 32 | ForEach-Object { [char]$_ })
$NrdpUrl = "http://$vmPublicIp/nrdp/"

# --- payload 1: Nagios Core 4 + NRDP -----------------------------------------
# Single-quoted here-string + .Replace(): the payload is dense with bash
# variables, so escaping every one with a backtick (the double-quoted pattern
# the other scripts use) would be a typo farm.
Write-Host "Installing Nagios Core 4 + NRDP $NrdpVersion on '$VmName'..."
$servicesLines = ($serviceNames -join "`n")
$payload = @'
#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y nagios4 monitoring-plugins-basic apache2 php php-xml libapache2-mod-php jq curl

# --- external commands (Debian README.Debian for nagios4) --------------------
# The rw dir must be enterable by www-data (NRDP writes to the command pipe).
dpkg-statoverride --update --add nagios www-data 2710 /var/lib/nagios4/rw 2>/dev/null || true
dpkg-statoverride --update --add nagios nagios 751 /var/lib/nagios4 2>/dev/null || true
# NRDP runs as www-data and hands results to Nagios by writing check-result
# files into Nagios' spool, so www-data must be in the nagios group. Without it
# the *parent* spool directory (0750 nagios:nagios) is not even traversable and
# every submission fails with "BAD CHECK RESULTS DIR" - which reads like the
# directory is missing, when really the permission chain stops one level above
# it. Apache picks the new group up at the restart at the end of this script.
usermod -a -G nagios www-data
sed -i 's/^check_external_commands=.*/check_external_commands=1/' /etc/nagios4/nagios.cfg
grep -q '^check_external_commands=1' /etc/nagios4/nagios.cfg || echo 'check_external_commands=1' >> /etc/nagios4/nagios.cfg

# --- Nagios UI auth ----------------------------------------------------------
# Ubuntu ships the CGIs with 'Require all granted' (or a 'Require ip' loopback
# list on older releases): no browser login, and the CGIs then refuse everything
# because REMOTE_USER is unset. Switch to digest auth as the nagiosadmin user.
htpw='__NAGIOS_PW__'
printf 'nagiosadmin:Nagios4:%s\n' "$(printf 'nagiosadmin:Nagios4:%s' "$htpw" | md5sum | cut -d' ' -f1)" > /etc/nagios4/htdigest.users
chown www-data:www-data /etc/nagios4/htdigest.users
chmod 640 /etc/nagios4/htdigest.users
# The packaged conf's layout differs between releases (auth directives at the
# DirectoryMatch level on some, inside <Files "cmd.cgi"> on others), so do not
# edit it in place: drop the loopback allow-list it may carry and add our own
# conf that applies digest auth to the whole Nagios tree. It sorts after
# nagios4-cgi.conf, and a later Require replaces an earlier one, so it wins.
sed -i -e 's/Require all[[:space:]]\+granted/Require valid-user/' -e '/Require ip/d' /etc/apache2/conf-available/nagios4-cgi.conf
cat > /etc/apache2/conf-available/nagios4-zz-fleet-auth.conf <<'EOF'
# Added by setup-nagios-machine.ps1: digest login for the Nagios UI and CGIs.
<DirectoryMatch (/usr/share/nagios4/htdocs|/usr/lib/cgi-bin/nagios4|/etc/nagios4/stylesheets)>
    AuthType Digest
    AuthName "Nagios4"
    AuthDigestDomain /nagios4/
    AuthDigestProvider file
    AuthUserFile /etc/nagios4/htdigest.users
    Require valid-user
</DirectoryMatch>
EOF
a2enmod cgid auth_digest authz_groupfile >/dev/null
a2enconf nagios4-cgi nagios4-zz-fleet-auth >/dev/null
# The packaged cgi.cfg trusts the (now removed) IP allow-list, so make the CGIs
# use the digest identity and authorize nagiosadmin for everything.
sed -i 's/^use_authentication=.*/use_authentication=1/' /etc/nagios4/cgi.cfg
for k in system_information configuration_information system_commands all_services all_hosts all_service_commands all_host_commands; do
    if grep -q "^authorized_for_$k=" /etc/nagios4/cgi.cfg; then
        sed -i "s/^authorized_for_$k=.*/authorized_for_$k=nagiosadmin/" /etc/nagios4/cgi.cfg
    else
        echo "authorized_for_$k=nagiosadmin" >> /etc/nagios4/cgi.cfg
    fi
done

# --- stock objects out of the way --------------------------------------------
# The packaged localhost/gateway objects run active checks that have nothing to
# do with the fleet estate; keep the generic templates and timeperiods.
for f in localhost_nagios2 services_nagios2 host-gateway_nagios3 hostgroups_nagios2 extinfo_nagios2; do
    [ ! -f "/etc/nagios4/conf.d/$f.cfg" ] || mv "/etc/nagios4/conf.d/$f.cfg" "/etc/nagios4/conf.d/$f.cfg.disabled"
done

# --- fleet templates + service catalog ---------------------------------------
plugin_dir=/usr/lib/nagios/plugins
cat > /etc/nagios4/conf.d/fleet-templates.cfg <<EOF
# Templates for hosts mirrored from the fleet server (fleet-nagios-sync).
define command {
    command_name fleet-host-up
    command_line $plugin_dir/check_dummy 0 "assumed up (passive-only host)"
}
define command {
    command_name fleet-stale
    command_line $plugin_dir/check_dummy 2 "stale - no passive result within __FRESHNESS__s"
}
define host {
    name                   fleet-host
    register               0
    check_command          fleet-host-up
    check_period           24x7
    max_check_attempts     1
    check_interval         5
    retry_interval         1
    active_checks_enabled  0
    passive_checks_enabled 1
    check_freshness        0
    notifications_enabled  0
    notification_interval  0
    notification_period    24x7
}
define service {
    name                   fleet-passive-service
    register               0
    check_command          fleet-stale
    check_period           24x7
    max_check_attempts     1
    check_interval         5
    retry_interval         1
    active_checks_enabled  0
    passive_checks_enabled 1
    check_freshness        1
    freshness_threshold    __FRESHNESS__
    notifications_enabled  0
    notification_interval  0
    notification_period    24x7
}
# Nagios refuses to start with zero hosts or zero services ("There are no
# hosts defined!"), and the fleet tree is legitimately empty before the first
# agent enrolls and after the last one leaves. This anchor keeps the config
# valid; the provisioning self-test submits to it (no freshness: nothing
# reports to it afterwards).
define host {
    use             fleet-host
    host_name       nrdp-selftest
    alias           NRDP self-test target (provisioning)
    address         127.0.0.1
}
define service {
    use                 fleet-passive-service
    host_name           nrdp-selftest
    service_description selftest
    check_freshness     0
}
EOF
cat > /etc/nagios4/fleet-services.list <<'EOF'
__SERVICES__
EOF
mkdir -p /etc/nagios4/conf.d/fleet

# --- NRDP --------------------------------------------------------------------
cd /tmp
curl -fsSL -o nrdp.tar.gz "https://github.com/NagiosEnterprises/nrdp/archive/refs/tags/__NRDP_VERSION__.tar.gz"
rm -rf /tmp/nrdp-src && mkdir /tmp/nrdp-src
tar -xzf nrdp.tar.gz -C /tmp/nrdp-src --strip-components=1
rm -rf /usr/local/nrdp
cp -r /tmp/nrdp-src/server /usr/local/nrdp
chown -R www-data:www-data /usr/local/nrdp

# Read the live paths from nagios.cfg instead of hard-coding the Debian layout.
command_file=$(grep '^command_file=' /etc/nagios4/nagios.cfg | cut -d= -f2)
check_result_path=$(grep '^check_result_path=' /etc/nagios4/nagios.cfg | cut -d= -f2)
if [ -z "$command_file" ] || [ -z "$check_result_path" ]; then
    echo "could not read command_file/check_result_path from nagios.cfg"
    exit 1
fi
# NRDP drops finished results into the checkresults spool as www-data.
# The spool has to be writable by www-data (it creates the result files) and
# readable by nagios (it reaps them). Group nagios + setgid does both: www-data
# reaches it through its nagios membership, and every file created lands in
# group nagios, which is what makes NRDP's own chmod 0770 readable by the
# daemon. NRDP tries to chgrp each file itself, but only when PHP has the posix
# extension - setgid means the result does not depend on that.
chgrp nagios "$check_result_path"
chmod 2770 "$check_result_path"

# config.inc.php: strip a closing tag if one exists, then append our settings
# (last assignment wins in PHP, so no fragile in-place edits of the defaults).
sed -i 's/^?>$//' /usr/local/nrdp/config.inc.php
cat >> /usr/local/nrdp/config.inc.php <<EOF

// --- appended by setup-nagios-machine.ps1 ---
\$cfg['authorized_tokens'][] = "__NRDP_TOKEN__";
\$cfg['command_file'] = "$command_file";
\$cfg['check_results_dir'] = "$check_result_path";
\$cfg['nagios_command_group'] = "nagios";
EOF

cat > /etc/apache2/conf-available/nrdp.conf <<'EOF'
Alias /nrdp /usr/local/nrdp
<Directory "/usr/local/nrdp">
    Options FollowSymLinks
    AllowOverride None
    Require all granted
</Directory>
EOF
a2enconf nrdp >/dev/null

# --- validate + start --------------------------------------------------------
nagios4 -v /etc/nagios4/nagios.cfg
systemctl restart apache2
systemctl restart nagios4
systemctl is-active --quiet nagios4
systemctl is-active --quiet apache2
echo "NAGIOS: installed"
'@
$payload = $payload.Replace('__NAGIOS_PW__', $NagiosPassword).Replace('__FRESHNESS__', "$freshnessThreshold").Replace('__SERVICES__', $servicesLines).Replace('__NRDP_VERSION__', $NrdpVersion).Replace('__NRDP_TOKEN__', $NrdpToken)
Invoke-VMStep -ScriptString $payload -Marker "NAGIOS: installed" -FailureMessage "Failed to install Nagios/NRDP on the VM (see the output above)."
Write-Host "✅ Nagios Core 4 + NRDP installed."

# --- payload 2: the registration poller --------------------------------------
if ($FleetServer) {
    Write-Host "Installing the fleet-nagios-sync timer (polling $FleetServer)..."
    $curlOpts = ""
    $caWrite = ""
    if ($FleetNoVerify) { $curlOpts = "--insecure" }
    if ($FleetCaFile) {
        $curlOpts = "--cacert /etc/fleet-nagios-ca.pem"
        $caWrite = "cat > /etc/fleet-nagios-ca.pem <<'NSCP_FLEET_CA_EOF'`n" + ((Get-Content -Path $FleetCaFile -Raw) -replace "`r", "").TrimEnd() + "`nNSCP_FLEET_CA_EOF"
    }
    $payload = @'
#!/bin/bash
set -e
__CA_WRITE__
cat > /usr/local/bin/fleet-nagios-sync <<'NSCP_SYNC_EOF'
__SYNC_SCRIPT__
NSCP_SYNC_EOF
chmod 755 /usr/local/bin/fleet-nagios-sync

cat > /etc/fleet-nagios-sync.conf <<EOF
FLEET_SERVER="__FLEET_SERVER__"
FLEET_API_KEY="__FLEET_API_KEY__"
CURL_OPTS="__CURL_OPTS__"
EOF
chmod 600 /etc/fleet-nagios-sync.conf

cat > /etc/systemd/system/fleet-nagios-sync.service <<'EOF'
[Unit]
Description=Mirror NSClient fleet hosts into Nagios
After=network-online.target nagios4.service

[Service]
Type=oneshot
ExecStart=/usr/local/bin/fleet-nagios-sync
EOF
cat > /etc/systemd/system/fleet-nagios-sync.timer <<'EOF'
[Unit]
Description=Run fleet-nagios-sync every minute

[Timer]
OnBootSec=1min
OnUnitActiveSec=1min

[Install]
WantedBy=timers.target
EOF
systemctl daemon-reload
systemctl enable --now fleet-nagios-sync.timer

# Run it once, now: a misconfigured key/url must fail the provisioning run,
# not sit silently broken in a timer. Through systemctl (not directly) so it
# is serialised with the run the timer may already have started - a oneshot
# start blocks until the unit finishes and reports its exit status.
if ! systemctl start fleet-nagios-sync.service; then
    journalctl -u fleet-nagios-sync --no-pager -n 30
    echo "SYNC: failed"
    exit 1
fi
journalctl -u fleet-nagios-sync --no-pager -n 5
echo "SYNC: ok"
'@
    $payload = $payload.Replace('__CA_WRITE__', $caWrite).Replace('__SYNC_SCRIPT__', $syncScript.TrimEnd()).Replace('__FLEET_SERVER__', $FleetServer).Replace('__FLEET_API_KEY__', $SyncApiKey).Replace('__CURL_OPTS__', $curlOpts)
    Invoke-VMStep -ScriptString $payload -Marker "SYNC: ok" -FailureMessage "fleet-nagios-sync did not complete on the VM - the fleet server may be unreachable from Azure, or the API key is wrong."
    Write-Host "✅ Host auto-registration is running (every minute)."
}
else {
    Write-Warning "No -FleetServer: skipping host auto-registration. Hosts must be added to /etc/nagios4/conf.d/fleet/ by hand."
}

# --- payload 3: self-test ----------------------------------------------------
Write-Host "Self-testing NRDP and the Nagios CGIs..."
$payload = @'
#!/bin/bash
set -e
status_json='http://localhost/cgi-bin/nagios4/statusjson.cgi'

# 1. The CGIs must demand a login: an unauthenticated statusjson answers 401,
#    not an empty (but "Success") host list.
code=$(curl -s -o /dev/null -w '%{http_code}' "$status_json?query=hostlist")
[ "$code" = "401" ] || { echo "statusjson.cgi answered $code without credentials (expected 401) - the CGI auth is not in effect"; exit 1; }

# 2. A passive result submitted through NRDP for the anchor host ...
xml="<?xml version='1.0'?><checkresults><checkresult type='service'><hostname>nrdp-selftest</hostname><servicename>selftest</servicename><state>0</state><output>selftest ok</output></checkresult></checkresults>"
out=$(curl -fsS -d "token=__NRDP_TOKEN__" -d "cmd=submitcheck" --data-urlencode "xml=$xml" http://localhost/nrdp/)
echo "$out"
echo "$out" | grep -q "<status>0</status>" || { echo "NRDP submit failed"; exit 1; }

# 3. ... must show up in Nagios as a *passive* check result (check_type 1),
#    proving the command file / check-result spool permissions and the
#    reaper are all working. The reaper runs every few seconds; allow a minute.
for i in $(seq 1 12); do
    if curl -fsS --digest -u 'nagiosadmin:__NAGIOS_PW__' "$status_json?query=service&hostname=nrdp-selftest&servicedescription=selftest" \
        | jq -e '.data.service.has_been_checked == true and .data.service.check_type == 1' >/dev/null 2>&1; then
        echo "NRDP: ok"
        exit 0
    fi
    sleep 5
done
echo "the NRDP result never reached Nagios (check journalctl -u nagios4 and /var/log/apache2/error.log)"
curl -fsS --digest -u 'nagiosadmin:__NAGIOS_PW__' "$status_json?query=service&hostname=nrdp-selftest&servicedescription=selftest" || true
exit 1
'@
$payload = $payload.Replace('__NRDP_TOKEN__', $NrdpToken).Replace('__NAGIOS_PW__', $NagiosPassword)
Invoke-VMStep -ScriptString $payload -Marker "NRDP: ok" -FailureMessage "The NRDP/CGI self-test failed on the VM (see the output above)."
Write-Host "✅ NRDP accepts results and the Nagios CGIs answer."

# --- credentials file --------------------------------------------------------
if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".nagios.pwd" }
$fleetLine = if ($FleetServer) { "`nFleet Server:   $FleetServer" } else { "" }
@"
VM Name:        $VmName
Resource Group: $ResourceGroupName
Public IP:      $vmPublicIp
SSH:            ssh -i $sshKeyPath $AdminUsername@$vmPublicIp
Admin Username: $AdminUsername
Nagios URL:     http://$vmPublicIp/nagios4/
Nagios User:    nagiosadmin
Nagios Password: $NagiosPassword
NRDP URL:       $NrdpUrl
NRDP Token:     $NrdpToken$fleetLine
"@ | Set-Content -Path $PwdFile -Force
Write-Host "● Credentials saved to $PwdFile"

Write-Host "✅ Script finished! Nagios is running at http://$vmPublicIp/nagios4/ (nagiosadmin)."
if ($FleetServer) {
    Write-Host "Enrolled fleet hosts appear in Nagios within a minute or two."
    Write-Host "Next: give the agents something to report with:"
    Write-Host "  ./build/powershell/nagios/add-nagios-bundle.ps1 -FleetServer $FleetServer"
}
