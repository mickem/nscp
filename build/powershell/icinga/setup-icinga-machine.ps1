#Requires -Module Az.Accounts
#Requires -Module Az.Compute
#Requires -Module Az.Network

<#
.SYNOPSIS
    Provision an Ubuntu VM in Azure running Icinga 2 with its REST API open, as
    the monitoring server NSClient++ agents submit passive check results to.
.DESCRIPTION
    Stage 1 of the turn-key Icinga flow (see README.md). Creates the VM,
    installs Ubuntu's icinga2 package, builds the Icinga CA and the node
    certificate, enables the API listener on port 5665 and defines

      - two ApiUsers: "nscp", which the agents submit with (it may process
        check results and query/create Host and Service objects, nothing
        else), and "admin" for you and verify-icinga-estate.ps1;
      - two templates, "fleet-host" and "fleet-passive-service", that the
        agents' IcingaClient names when it creates its own host and service
        objects (`ensure objects = true`).

    Nothing registers hosts on this box: each agent creates its Host and its
    Services through the API the first time it submits. That is the part of
    IcingaClient this flow exists to exercise, and why no fleet API key is
    needed here.

    The templates give every object freshness checking the Icinga way: the
    check command is "dummy" and the check interval is three submission
    intervals, so an agent that stops reporting has its objects go
    DOWN/CRITICAL ("stale") when Icinga runs the dummy itself.

    Unless -NoWeb is given it then adds Icinga Web 2 on port 80 (plain http),
    fed by Icinga DB: the core streams its state into Redis, the icingadb
    daemon syncs it into MariaDB and the web UI's Icinga DB module reads it.
    All of it is Ubuntu 24.04's own packages. The web login is a separate
    "admin" user with its own password.

    Everything a later stage needs (address, both API users, the web login)
    is written to .icinga.pwd next to the other scripts' .vm.pwd files.

    The API serves a certificate issued by the VM's own Icinga CA and the
    agents are told not to verify it (see add-icinga-bundle.ps1), so the
    "nscp" password is sent to whoever answers at that address - fine for a
    throwaway test estate, not for anything else.
.PARAMETER ResourceGroupName
    The name for the new resource group.
.PARAMETER Location
    The Azure region where resources will be deployed.
.PARAMETER VmName
    The name for the new virtual machine. It is also Icinga's NodeName and the
    common name of the API certificate.
.PARAMETER UbuntuVersion
    The Ubuntu version to deploy ("24.04" or "22.04"; both package icinga2).
.PARAMETER AdminUsername
    The administrator username for the new VM.
.PARAMETER VmSize
    Azure VM size. The default is a 1-vCPU size: a subscription's vCPU quota is
    what caps the size of an estate, so halving the vCPUs per machine doubles
    how many fit. Must be a Generation 2 size (every image used here is Gen2).
.PARAMETER PwdFile
    Where to write the credentials file (default: build/powershell/.icinga.pwd).
.PARAMETER NoWeb
    Skip Icinga Web 2: no MariaDB, Redis, Icinga DB, Apache or port 80, just
    the core and its API. Needed with -UbuntuVersion 22.04 if that release
    does not package icingadb.
.PARAMETER AddWeb
    Do not create anything: install Icinga Web 2 on the VM an earlier run left
    in the credentials file, open port 80 on its security group and add the
    web login to that file. The VM keeps its address, so agents that already
    report to it are not disturbed.
.EXAMPLE
    ./setup-icinga-machine.ps1 -AddWeb
.EXAMPLE
    ./setup-icinga-machine.ps1
#>
param(
    [string]$ResourceGroupName = "NSCP-Icinga-RG",
    [string]$Location = "WestEurope",
    [string]$VmName = "NSCP-Icinga",
    [string]$UbuntuVersion = "24.04",
    [string]$AdminUsername = "azureadmin",
    [string]$VmSize = "Standard_F1as_v7",
    [string]$PwdFile = "",
    [switch]$NoWeb,
    [switch]$AddWeb
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")
Import-FleetConfig
# Only recorded in .icinga.pwd, so the later stages find the fleet server the
# estate belongs to; this script never talks to it.
$FleetServer = $env:NSCLIENT_FLEET_SERVER

# --- parameter validation, before anything costs money -----------------------
# The name goes into Icinga's configuration as a string constant and into the
# certificate as its common name.
if ($VmName -notmatch '^[A-Za-z0-9][A-Za-z0-9-]*$') {
    Write-Error "❌ -VmName '$VmName' must be letters, digits and '-' only."
    exit 1
}

# The catalog is the single source for the interval: the bundle submits every
# <interval>, so an object that has heard nothing for three of them is stale.
# Note the VM freezes that window at provisioning time - re-run this script (or
# edit /etc/icinga2/conf.d/fleet-templates.conf) after changing the interval.
$catalog = Read-PassiveCheckCatalog -Path (Join-Path $scriptDir "passive-checks.json")
$freshnessSeconds = 3 * $catalog.IntervalSeconds

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

if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".icinga.pwd" }

# --- Icinga Web 2 + Icinga DB (the last step of a run) -------------------------------------
# The web UI reads from a database, not from the core: Icinga 2 streams its
# state into Redis (the icingadb feature), the icingadb daemon syncs that into
# MariaDB, and Icinga Web 2's Icinga DB module reads both. Everything comes
# from Ubuntu's own repositories, same as icinga2 itself.
#
# A function because it has two callers: the tail of a full run, and -AddWeb,
# which bolts the UI onto a VM an earlier run left behind. Returns the lines to
# put in .icinga.pwd; exits when the install fails.
function Install-IcingaWeb {
    param(
        [Parameter(Mandatory)][string]$PublicIp,
        [Parameter(Mandatory)][string]$AdminApiUser,
        [Parameter(Mandatory)][string]$AdminApiPassword
    )
    $WebAdminUser = "admin"
    # Letters and digits only, as for the API passwords: it is pasted into a
    # bash single-quoted string.
    $WebAdminPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 24 | ForEach-Object { [char]$_ })
    $IcingaWebUrl = "http://${PublicIp}/icingaweb2/"
    Write-Host "Installing Icinga Web 2 and Icinga DB on '$VmName'..."
    $payload = @'
#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
if ! apt-cache show icingadb icingadb-web >/dev/null 2>&1; then
    echo "this Ubuntu release does not package icingadb / icingadb-web (24.04 does) - re-run with -NoWeb or -UbuntuVersion 24.04"
    exit 1
fi
apt-get install -y mariadb-server redis-server icingadb icingaweb2 icingadb-web icingacli \
    apache2 libapache2-mod-php php-mysql php-curl php-gd php-intl php-mbstring php-xml openssl
systemctl enable --now mariadb redis-server

# --- databases ---------------------------------------------------------------
# The database passwords never leave this box, so they are made here. The
# icingadb user exists for the socket ('localhost', what PHP uses) and for TCP
# ('127.0.0.1', what the icingadb daemon uses).
db_pw=$(openssl rand -hex 16)
web_db_pw=$(openssl rand -hex 16)
mysql <<EOF
CREATE DATABASE IF NOT EXISTS icingadb;
CREATE DATABASE IF NOT EXISTS icingaweb2;
CREATE OR REPLACE USER 'icingadb'@'localhost' IDENTIFIED BY '$db_pw';
CREATE OR REPLACE USER 'icingadb'@'127.0.0.1' IDENTIFIED BY '$db_pw';
CREATE OR REPLACE USER 'icingaweb2'@'localhost' IDENTIFIED BY '$web_db_pw';
GRANT ALL ON icingadb.* TO 'icingadb'@'localhost';
GRANT ALL ON icingadb.* TO 'icingadb'@'127.0.0.1';
GRANT ALL ON icingaweb2.* TO 'icingaweb2'@'localhost';
FLUSH PRIVILEGES;
EOF
[ -n "$(mysql -N -e "SHOW TABLES FROM icingadb LIKE 'icingadb_schema'")" ] || mysql icingadb < /usr/share/icingadb/schema/mysql/schema.sql
[ -n "$(mysql -N -e "SHOW TABLES FROM icingaweb2 LIKE 'icingaweb_user'")" ] || mysql icingaweb2 < /usr/share/icingaweb2/schema/mysql.schema.sql

# --- icinga2 -> redis -> icingadb -> mariadb ---------------------------------
# Both ends default to port 6380, where Icinga's own icingadb-redis package
# listens; Ubuntu's redis-server is on 6379.
cat > /etc/icinga2/features-available/icingadb.conf <<'EOF'
object IcingaDB "icingadb" {
  host = "127.0.0.1"
  port = 6379
}
EOF
icinga2 feature enable icingadb
icinga2 daemon -C
systemctl restart icinga2

cat > /etc/icingadb/config.yml <<EOF
# Written by setup-icinga-machine.ps1.
database:
  type: mysql
  host: 127.0.0.1
  port: 3306
  database: icingadb
  user: icingadb
  password: "$db_pw"
redis:
  host: 127.0.0.1
  port: 6379
EOF
chown icingadb:icingadb /etc/icingadb/config.yml
chmod 640 /etc/icingadb/config.yml
systemctl enable icingadb >/dev/null 2>&1 || true
systemctl restart icingadb || { echo "the icingadb daemon did not start"; journalctl -u icingadb --no-pager -n 30; exit 1; }

# --- Icinga Web 2 ------------------------------------------------------------
# The configuration the setup wizard would write, minus the wizard.
conf=/etc/icingaweb2
mkdir -p "$conf/modules/icingadb" "$conf/enabledModules"
cat > "$conf/resources.ini" <<EOF
[icingaweb2]
type = "db"
db = "mysql"
host = "localhost"
dbname = "icingaweb2"
username = "icingaweb2"
password = "$web_db_pw"
charset = "utf8mb4"

[icingadb]
type = "db"
db = "mysql"
host = "localhost"
dbname = "icingadb"
username = "icingadb"
password = "$db_pw"
charset = "utf8mb4"
EOF
cat > "$conf/config.ini" <<'EOF'
[global]
show_stacktraces = "0"
config_resource = "icingaweb2"

[logging]
log = "syslog"
level = "ERROR"
application = "icingaweb2"
EOF
cat > "$conf/authentication.ini" <<'EOF'
[icingaweb2]
backend = "db"
resource = "icingaweb2"
EOF
cat > "$conf/groups.ini" <<'EOF'
[icingaweb2]
backend = "db"
resource = "icingaweb2"
EOF
cat > "$conf/roles.ini" <<'EOF'
[Administrators]
users = "__WEB_USER__"
permissions = "*"
EOF
cat > "$conf/modules/icingadb/config.ini" <<'EOF'
[icingadb]
resource = "icingadb"
EOF
cat > "$conf/modules/icingadb/redis.ini" <<'EOF'
[redis1]
host = "127.0.0.1"
port = "6379"
EOF
# What "Check now", acknowledge and friends are sent through.
cat > "$conf/modules/icingadb/commandtransports.ini" <<'EOF'
[icinga2]
transport = "api"
host = "127.0.0.1"
port = "5665"
username = "__ADMIN_USER__"
password = "__ADMIN_PW__"
EOF
ln -sfn /usr/share/icingaweb2/modules/icingadb "$conf/enabledModules/icingadb"
chown -R root:icingaweb2 "$conf"
find "$conf" -type d -exec chmod 2770 {} +
find "$conf" -type f -exec chmod 660 {} +

# The login. password_hash() is what Icinga Web's own user form stores.
hash=$(php -r 'echo password_hash($argv[1], PASSWORD_DEFAULT);' '__WEB_PW__')
mysql icingaweb2 <<EOF
INSERT INTO icingaweb_user (name, active, password_hash, ctime) VALUES ('__WEB_USER__', 1, '$hash', NOW())
  ON DUPLICATE KEY UPDATE active = 1, password_hash = VALUES(password_hash);
EOF

# --- apache ------------------------------------------------------------------
# icingaweb2's postinst only enables these when apache2 was configured before
# it in the same apt run, which is not something to depend on.
a2enmod rewrite >/dev/null
a2enconf icingaweb2 >/dev/null
cat > /etc/apache2/conf-available/icingaweb2-root.conf <<'EOF'
RedirectMatch ^/$ /icingaweb2/
EOF
a2enconf icingaweb2-root >/dev/null
systemctl enable apache2 >/dev/null 2>&1 || true
systemctl restart apache2

# --- verify ------------------------------------------------------------------
# 1. The whole chain: Icinga 2 writes a heartbeat to Redis and the icingadb
#    daemon turns it into a row here. No row means the UI would show nothing.
ok=""
for i in $(seq 1 30); do
    rows=$(mysql -N -e "SELECT COUNT(*) FROM icingadb.icingadb_instance" 2>/dev/null || echo 0)
    if [ "$rows" -ge 1 ] 2>/dev/null; then ok=1; break; fi
    sleep 4
done
if [ -z "$ok" ]; then
    echo "Icinga 2 state never reached the Icinga DB database"
    systemctl --no-pager status icingadb redis-server | head -n 30 || true
    journalctl -u icingadb --no-pager -n 30 || true
    exit 1
fi

# 2. The login page renders (PHP, the rewrite rules and the config directory).
#    With a cookie jar and following redirects, like a browser: a first request
#    without cookies is answered with a 302 back to the same page, which is
#    Icinga Web testing that the client keeps cookies at all.
jar=$(mktemp)
pagefile=$(mktemp)
code=$(curl -s -L -b "$jar" -c "$jar" -o "$pagefile" -w '%{http_code}' http://localhost/icingaweb2/authentication/login || true)
page=$(cat "$pagefile"); rm -f "$pagefile"
if [ "$code" != "200" ] || ! echo "$page" | grep -q 'name="password"'; then
    echo "the Icinga Web 2 login page did not render (HTTP $code)"
    echo "$page" | head -c 600
    tail -n 30 /var/log/apache2/error.log || true
    exit 1
fi

# 3. Log in and open the host list. Reported, not required: it scrapes the
#    login form, and a UI that changed its markup is not a broken install.
token=$(echo "$page" | grep -o '<input[^>]*CSRFToken[^>]*>' | head -n 1 | sed -n 's/.*value="\([^"]*\)".*/\1/p')
curl -s -o /dev/null -b "$jar" -c "$jar" \
    --data-urlencode 'username=__WEB_USER__' --data-urlencode 'password=__WEB_PW__' \
    --data-urlencode "CSRFToken=$token" --data-urlencode 'formUID=form_login' --data-urlencode 'btn_submit=Login' \
    http://localhost/icingaweb2/authentication/login || true
code=$(curl -s -o /dev/null -w '%{http_code}' -b "$jar" -H 'Accept: application/json' 'http://localhost/icingaweb2/icingadb/hosts?format=json' || true)
rm -f "$jar"
if [ "$code" = "200" ]; then echo "ICINGA-WEB-LOGIN: ok"; else echo "ICINGA-WEB-LOGIN: unverified (host list answered HTTP $code)"; fi

echo "ICINGA-WEB: installed"
'@
    $payload = $payload.Replace('__WEB_USER__', $WebAdminUser).Replace('__WEB_PW__', $WebAdminPassword).
    Replace('__ADMIN_USER__', $AdminApiUser).Replace('__ADMIN_PW__', $AdminApiPassword)
    # Not Invoke-VMStep: the login line decides which message to print.
    $payload = $payload -replace "`r", ""
    $result = Invoke-VMRun -ResourceGroupName $ResourceGroupName -VMName $VmName -ScriptString $payload
    $messages = @($result.Value | ForEach-Object { $_.Message })
    # To the host, not the pipeline: this function's output is its return value.
    $messages | ForEach-Object { Write-Host $_ }
    if ($result.Status -ne "Succeeded" -or -not ($messages -match "ICINGA-WEB: installed")) {
        Write-Error "❌ Failed to install Icinga Web 2 on the VM (see the output above). The Icinga 2 API itself is up and its credentials are saved; retry the UI alone with -AddWeb."
        exit 1
    }
    if ($messages -match "ICINGA-WEB-LOGIN: ok") {
        Write-Host "✅ Icinga Web 2 installed; logged in as '$WebAdminUser' and opened the host list."
    }
    else {
        Write-Warning "Icinga Web 2 is installed and its login page answers, but the scripted login could not be confirmed - try it in a browser."
    }
    Write-Host "   Icinga Web 2: $IcingaWebUrl (user '$WebAdminUser')"
    return @(
        "Icinga Web URL: $IcingaWebUrl",
        "Icinga Web User: $WebAdminUser",
        "Icinga Web Password: $WebAdminPassword"
    )
}

# --- -AddWeb: the UI for a VM that already runs Icinga 2 ----------------------
if ($AddWeb) {
    if (-not (Test-Path $PwdFile)) {
        Write-Error "❌ -AddWeb needs the credentials file of an earlier run, and '$PwdFile' does not exist."
        exit 1
    }
    $existing = @{}
    foreach ($line in Get-Content -Path $PwdFile) {
        if ($line -match '^([^:]+):\s*(.*)$') { $existing[$Matches[1].Trim()] = $Matches[2].Trim() }
    }
    foreach ($key in "VM Name", "Resource Group", "Public IP", "Icinga Admin User", "Icinga Admin Password") {
        if (-not $existing[$key]) {
            Write-Error "❌ '$PwdFile' has no '$key' line - it was not written by this script."
            exit 1
        }
    }
    # The VM the file names, not the parameter defaults.
    $VmName = $existing["VM Name"]
    $ResourceGroupName = $existing["Resource Group"]

    # The file outlives the VM it describes (a teardown does not remove it).
    if (-not (Get-AzVM -ResourceGroupName $ResourceGroupName -Name $VmName -ErrorAction SilentlyContinue)) {
        Write-Error "❌ '$PwdFile' names VM '$VmName' in resource group '$ResourceGroupName', which no longer exists - there is nothing to add the UI to. Run this script without -AddWeb: a full run installs Icinga Web 2 as well."
        exit 1
    }

    Write-Host "Opening port 80 on '$VmName'..."
    $nsg = Get-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName
    if (-not ($nsg.SecurityRules | Where-Object { $_.Name -eq "Allow-Icinga-Web" })) {
        $nsg | Add-AzNetworkSecurityRuleConfig -Name "Allow-Icinga-Web" -Protocol Tcp -Direction Inbound -Priority 1020 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "80" -Access Allow | Set-AzNetworkSecurityGroup | Out-Null
    }

    $webLines = Install-IcingaWeb -PublicIp $existing["Public IP"] -AdminApiUser $existing["Icinga Admin User"] -AdminApiPassword $existing["Icinga Admin Password"]
    # Replace the web lines of a previous -AddWeb, keep everything else.
    $kept = @(Get-Content -Path $PwdFile | Where-Object { $_ -notmatch '^Icinga Web ' })
    ($kept + $webLines) | Set-Content -Path $PwdFile -Force
    Write-Host "● Credentials saved to $PwdFile"
    exit 0
}
# --- SSH key (same conventions as the other setup scripts) -------------------
$sshDir = Join-Path $HOME ".ssh"
if (-not (Test-Path $sshDir)) { New-Item -ItemType Directory -Path $sshDir -Force | Out-Null }
$sshKeyPath = Join-Path $sshDir "az_$($VmName)_rsa"
if (-not (Test-Path $sshKeyPath)) {
    Write-Host "Generating SSH key pair..."
    # No passphrase. Windows PowerShell (and 7 in Legacy argument mode) drops
    # an empty string argument to a native command, so a plain -N '' made
    # ssh-keygen take the next switch, "-q", as the passphrase.
    $noPassphrase = if ($PSNativeCommandArgumentPassing -in 'Standard', 'Windows') { '' } else { '""' }
    ssh-keygen -t rsa -b 4096 -f $sshKeyPath -N $noPassphrase -q
}
$sshPublicKey = Get-Content "$sshKeyPath.pub"

# --- Azure resources ---------------------------------------------------------
Write-Host "Creating resource group: $ResourceGroupName..."
New-AzResourceGroup -Name $ResourceGroupName -Location $Location -Force

Write-Host "Configuring virtual network and security rules..."
$subnetConfig = New-AzVirtualNetworkSubnetConfig -Name "default-subnet" -AddressPrefix "10.0.0.0/24"
$vnet = New-AzVirtualNetwork -Name "$($VmName)-vnet" -ResourceGroupName $ResourceGroupName -Location $Location -AddressPrefix "10.0.0.0/16" -Subnet $subnetConfig
$publicIp = New-AzPublicIpAddress -Name "$($VmName)-pip" -ResourceGroupName $ResourceGroupName -Location $Location -AllocationMethod Static -Sku Standard

# 22 for SSH, 5665 for the Icinga 2 REST API - the agents submit their passive
# results there, and verify-icinga-estate.ps1 reads the state back from it.
$nsgRuleSSH = New-AzNetworkSecurityRuleConfig -Name "Allow-SSH" -Protocol Tcp -Direction Inbound -Priority 1000 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "22" -Access Allow
$nsgRuleApi = New-AzNetworkSecurityRuleConfig -Name "Allow-Icinga-API" -Protocol Tcp -Direction Inbound -Priority 1010 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "5665" -Access Allow
$nsgRules = @($nsgRuleSSH, $nsgRuleApi)
if (-not $NoWeb) {
    # 80 for Icinga Web 2. Plain http: there is no certificate a browser would
    # trust for a bare address anyway.
    $nsgRules += New-AzNetworkSecurityRuleConfig -Name "Allow-Icinga-Web" -Protocol Tcp -Direction Inbound -Priority 1020 -SourceAddressPrefix "*" -SourcePortRange "*" -DestinationAddressPrefix "*" -DestinationPortRange "80" -Access Allow
}
$nsg = New-AzNetworkSecurityGroup -Name "$($VmName)-nsg" -ResourceGroupName $ResourceGroupName -Location $Location -SecurityRules $nsgRules

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
# Letters and digits only: both passwords are pasted into an Icinga string
# literal, a bash single-quoted string and (the agent one) an INI value.
$AdminApiPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 24 | ForEach-Object { [char]$_ })
$AgentApiPassword = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 24 | ForEach-Object { [char]$_ })
$AdminApiUser = "admin"
$AgentApiUser = "nscp"
$IcingaUrl = "https://${vmPublicIp}:5665/"

# --- payload 1: Icinga 2 + the API listener ----------------------------------
# Single-quoted here-string + .Replace(): the payload is dense with bash
# variables, so escaping every one with a backtick (the double-quoted pattern
# the other scripts use) would be a typo farm.
Write-Host "Installing Icinga 2 on '$VmName'..."
$payload = @'
#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y icinga2 jq curl openssl
# The package starts the daemon on its stock configuration; it comes back
# below, on ours.
systemctl stop icinga2 || true

NODE_NAME='__NODE_NAME__'
CERT_DIR=/var/lib/icinga2/certs
# The Debian/Ubuntu packages run the daemon as nagios; Icinga's own as icinga.
if id nagios >/dev/null 2>&1; then icinga_user=nagios; else icinga_user=icinga; fi

# --- CA + node certificate ---------------------------------------------------
# One `icinga2 pki` call per step instead of `icinga2 api setup`, which stalls
# after writing the CSR when it has no terminal (tests/Dockerfiles/entrypoints/
# icinga.sh hit the same thing). They run as the daemon user, which must be
# able to read back the 0600 key files it writes, through setpriv rather than
# runuser: no PAM, no session, nothing that waits on a TTY RunCommand does not
# have. The timeout turns any stall that is left into a failed step.
mkdir -p /var/lib/icinga2/ca "$CERT_DIR"
chown -R "$icinga_user:$icinga_user" /var/lib/icinga2/ca "$CERT_DIR"
as_icinga() {
    timeout 120 setpriv --reuid="$icinga_user" --regid="$icinga_user" --init-groups -- "$@" </dev/null
}
[ -f /var/lib/icinga2/ca/ca.crt ] || as_icinga icinga2 pki new-ca
as_icinga icinga2 pki new-cert --cn "$NODE_NAME" --key "$CERT_DIR/$NODE_NAME.key" --csr "$CERT_DIR/$NODE_NAME.csr"
as_icinga icinga2 pki sign-csr --csr "$CERT_DIR/$NODE_NAME.csr" --cert "$CERT_DIR/$NODE_NAME.crt"
as_icinga cp /var/lib/icinga2/ca/ca.crt "$CERT_DIR/ca.crt"

# --- node identity + API listener --------------------------------------------
# NodeName must be the certificate's common name: the listener loads
# certs/<NodeName>.crt. Drop whatever the stock file sets, then pin both.
#
# TicketSalt is not needed by the passive flow, but ships empty, and with an
# empty salt `icinga2 pki ticket` refuses - so an agent (docs: scenarios/
# icinga-agent.md) could not be enrolled against this box without it. The
# zones.d folder is where that agent's host and services go; it must carry the
# zone's name, which here is the node's, not "master".
sed -i -e '/^const NodeName/d' -e '/^const ZoneName/d' -e '/^const TicketSalt/d' /etc/icinga2/constants.conf
cat >> /etc/icinga2/constants.conf <<EOF

const NodeName = "$NODE_NAME"
const ZoneName = "$NODE_NAME"
const TicketSalt = "$(openssl rand -hex 16)"
EOF
mkdir -p "/etc/icinga2/zones.d/$NODE_NAME"
chown "$icinga_user:$icinga_user" "/etc/icinga2/zones.d/$NODE_NAME"

# The listener refuses to start without an Endpoint for the local node that is
# a member of a Zone. Single node: one of each.
cat > /etc/icinga2/zones.conf <<'EOF'
object Endpoint NodeName {
}
object Zone ZoneName {
  endpoints = [ NodeName ]
}
EOF

cat > /etc/icinga2/features-available/api.conf <<'EOF'
object ApiListener "api" {
  bind_host = "0.0.0.0"
  bind_port = 5665
}
EOF
icinga2 feature enable api

# --- API users ---------------------------------------------------------------
# The agents' user can do exactly what IcingaClient does with
# `ensure objects = true`: look a host or service up, create it when it is
# missing, and submit its result. It cannot delete or modify anything, which
# matters because its password travels to every machine in the bundle.
cat > /etc/icinga2/conf.d/api-users.conf <<'EOF'
// Written by setup-icinga-machine.ps1.
object ApiUser "__AGENT_USER__" {
  password = "__AGENT_PW__"
  permissions = [
    "actions/process-check-result",
    "objects/query/Host",
    "objects/query/Service",
    "objects/create/Host",
    "objects/create/Service"
  ]
}

object ApiUser "__ADMIN_USER__" {
  password = "__ADMIN_PW__"
  permissions = [ "*" ]
}
EOF
chown "root:$icinga_user" /etc/icinga2/conf.d/api-users.conf
chmod 640 /etc/icinga2/conf.d/api-users.conf

# --- stock objects out of the way --------------------------------------------
# The packaged samples monitor the Icinga box itself with active plugin checks
# and attach apply rules to anything with an address - which every host an
# agent creates has.
for f in hosts services users groups notifications; do
    [ ! -f "/etc/icinga2/conf.d/$f.conf" ] || mv "/etc/icinga2/conf.d/$f.conf" "/etc/icinga2/conf.d/$f.conf.disabled"
done

# --- the templates the agents create their objects from ----------------------
# Freshness, the Icinga 2 way: there is no freshness_threshold. A passive
# result pushes the next check out by check_interval; when nothing arrives in
# that time Icinga runs the check command itself, and "dummy" then reports the
# object as stale. Active checks therefore stay enabled - switching them off
# would switch the freshness check off with them.
cat > /etc/icinga2/conf.d/fleet-templates.conf <<'EOF'
// Written by setup-icinga-machine.ps1. NSClient++ agents name these templates
// when IcingaClient creates their Host and Service objects.
template Host "fleet-host" {
  check_command = "dummy"
  max_check_attempts = 1
  check_interval = __FRESHNESS__s
  retry_interval = __FRESHNESS__s
  enable_passive_checks = true
  vars.dummy_state = 2
  vars.dummy_text = "stale - no passive host_check result within __FRESHNESS__s"
}

template Service "fleet-passive-service" {
  check_command = "dummy"
  max_check_attempts = 1
  check_interval = __FRESHNESS__s
  retry_interval = __FRESHNESS__s
  enable_passive_checks = true
  vars.dummy_state = 2
  vars.dummy_text = "stale - no passive result within __FRESHNESS__s"
}
EOF

# --- validate + start --------------------------------------------------------
icinga2 daemon -C
systemctl enable icinga2 >/dev/null 2>&1 || true
systemctl restart icinga2
systemctl is-active --quiet icinga2

# The unit is active well before the listener answers.
for i in $(seq 1 30); do
    code=$(curl -ks -o /dev/null -w '%{http_code}' -u '__ADMIN_USER__:__ADMIN_PW__' -H 'Accept: application/json' https://localhost:5665/v1/status || true)
    if [ "$code" = "200" ]; then
        icinga2 --version | head -n 1
        echo "ICINGA: installed"
        exit 0
    fi
    sleep 2
done
echo "the Icinga 2 API never answered on port 5665 (last status: $code)"
journalctl -u icinga2 --no-pager -n 40
exit 1
'@
$payload = $payload.Replace('__NODE_NAME__', $VmName).Replace('__FRESHNESS__', "$freshnessSeconds").
Replace('__AGENT_USER__', $AgentApiUser).Replace('__AGENT_PW__', $AgentApiPassword).
Replace('__ADMIN_USER__', $AdminApiUser).Replace('__ADMIN_PW__', $AdminApiPassword)
Invoke-VMStep -ScriptString $payload -Marker "ICINGA: installed" -FailureMessage "Failed to install Icinga 2 on the VM (see the output above)."
Write-Host "✅ Icinga 2 installed, API listening on 5665."

# --- payload 2: self-test ----------------------------------------------------
# Walk the agent's own path with the agent's own user: create a host and a
# service from the templates, submit a result, read it back as a passive one.
# A template that does not validate or a permission that is one word off fails
# here, in a minute, instead of as an estate that never reports.
Write-Host "Self-testing the Icinga 2 API..."
$payload = @'
#!/bin/bash
set -e
api=https://localhost:5665/v1
agent='__AGENT_USER__:__AGENT_PW__'
admin='__ADMIN_USER__:__ADMIN_PW__'
host=icinga-selftest
# What `curl -f` hides is the one thing worth reading when a step fails.
call() {
    local user=$1 method=$2 path=$3 body=$4 out code
    out=$(mktemp)
    if [ -n "$body" ]; then
        code=$(curl -ks -o "$out" -w '%{http_code}' -u "$user" -X "$method" -H 'Accept: application/json' -H 'Content-Type: application/json' -d "$body" "$api$path")
    else
        code=$(curl -ks -o "$out" -w '%{http_code}' -u "$user" -X "$method" -H 'Accept: application/json' "$api$path")
    fi
    cat "$out"; rm -f "$out"
    case "$code" in 2*) return 0 ;; esac
    echo; echo "$method $path answered HTTP $code" >&2
    return 1
}

# 1. The API must demand a login.
code=$(curl -ks -o /dev/null -w '%{http_code}' -H 'Accept: application/json' "$api/status")
[ "$code" = "401" ] || { echo "/v1/status answered $code without credentials (expected 401)"; exit 1; }

# A previous run's leftovers would make the creates below fail.
curl -ks -o /dev/null -u "$admin" -X DELETE -H 'Accept: application/json' "$api/objects/hosts/$host?cascade=1" || true

# 2. Create a host and a service the way IcingaClient does ...
call "$agent" PUT "/objects/hosts/$host" '{"templates":["fleet-host"],"attrs":{"address":"icinga-selftest"}}' >/dev/null
call "$agent" PUT "/objects/services/$host%21selftest" '{"templates":["fleet-passive-service"],"attrs":{"check_command":"dummy"}}' >/dev/null

# 3. ... submit a service and a host result ...
out=$(call "$agent" POST /actions/process-check-result '{"type":"Service","filter":"host.name==\"icinga-selftest\" && service.name==\"selftest\"","exit_status":0,"plugin_output":"selftest ok","performance_data":["answer=42"],"check_source":"icinga-selftest"}')
echo "$out" | jq -e '.results[0].code == 200' >/dev/null || { echo "the service result was not accepted: $out"; exit 1; }
out=$(call "$agent" POST /actions/process-check-result '{"type":"Host","filter":"host.name==\"icinga-selftest\"","exit_status":0,"plugin_output":"selftest up","check_source":"icinga-selftest"}')
echo "$out" | jq -e '.results[0].code == 200' >/dev/null || { echo "the host result was not accepted: $out"; exit 1; }

# 4. ... and both must be the object's last result, as *passive* results
#    (active == false): the freshness dummy sets a last_check_result too, and
#    that one must not be mistaken for an agent reporting.
ok=""
for i in $(seq 1 12); do
    service=$(call "$admin" GET "/objects/services/$host%21selftest" "" || true)
    hostobj=$(call "$admin" GET "/objects/hosts/$host" "" || true)
    if echo "$service" | jq -e '.results[0].attrs.last_check_result | (.active == false and .output == "selftest ok")' >/dev/null 2>&1 \
        && echo "$hostobj" | jq -e '.results[0].attrs.last_check_result | (.active == false and .output == "selftest up")' >/dev/null 2>&1; then
        ok=1
        break
    fi
    sleep 5
done
if [ -z "$ok" ]; then
    echo "the submitted results never became the objects' last check result"
    echo "$service" | jq '.results[0].attrs.last_check_result' || echo "$service"
    echo "$hostobj" | jq '.results[0].attrs.last_check_result' || echo "$hostobj"
    exit 1
fi

# 5. The agents' user must not be able to remove what it created.
code=$(curl -ks -o /dev/null -w '%{http_code}' -u "$agent" -X DELETE -H 'Accept: application/json' "$api/objects/hosts/$host?cascade=1")
case "$code" in 2*) echo "the agents' API user may delete objects (HTTP $code) - its permissions are too wide"; exit 1 ;; esac

call "$admin" DELETE "/objects/hosts/$host?cascade=1" "" >/dev/null
echo "ICINGA-API: ok"
'@
$payload = $payload.Replace('__AGENT_USER__', $AgentApiUser).Replace('__AGENT_PW__', $AgentApiPassword).
Replace('__ADMIN_USER__', $AdminApiUser).Replace('__ADMIN_PW__', $AdminApiPassword)
Invoke-VMStep -ScriptString $payload -Marker "ICINGA-API: ok" -FailureMessage "The Icinga 2 API self-test failed on the VM (see the output above)."
Write-Host "✅ The Icinga 2 API creates objects from the fleet templates and accepts passive results."

# --- credentials file --------------------------------------------------------
# Written before the web UI goes on: a failed UI install must not cost the
# credentials of an Icinga 2 that works (and -AddWeb can then retry it).
$fleetLine = if ($FleetServer) { "`nFleet Server:   $FleetServer" } else { "" }
@"
VM Name:        $VmName
Resource Group: $ResourceGroupName
Public IP:      $vmPublicIp
SSH:            ssh -i $sshKeyPath $AdminUsername@$vmPublicIp
Admin Username: $AdminUsername
Icinga URL:     $IcingaUrl
Icinga Admin User: $AdminApiUser
Icinga Admin Password: $AdminApiPassword
Icinga Agent User: $AgentApiUser
Icinga Agent Password: $AgentApiPassword$fleetLine
"@ | Set-Content -Path $PwdFile -Force
Write-Host "● Credentials saved to $PwdFile"

$webLines = @()
if (-not $NoWeb) {
    $webLines = Install-IcingaWeb -PublicIp $vmPublicIp -AdminApiUser $AdminApiUser -AdminApiPassword $AdminApiPassword
    $webLines | Add-Content -Path $PwdFile
}

Write-Host "✅ Script finished! Icinga 2 is answering at ${IcingaUrl}v1 (API user '$AdminApiUser')."
if ($webLines) { Write-Host "   The Icinga Web 2 login is in $PwdFile." }
Write-Host "Next: give the agents something to report with:"
Write-Host "  ./build/powershell/icinga/add-icinga-bundle.ps1"
