<#
.SYNOPSIS
    Verify the turn-key monitoring flow end to end: every enrolled fleet host is
    registered in Nagios and its passive services have received results.
.DESCRIPTION
    Stage 4 of the turn-key monitoring flow (see README.md). Reads the enrolled
    hosts from the fleet server (GET /api/hosts) and the monitoring state from
    the Nagios VM's statusjson.cgi, then waits (up to -WaitMinutes) until

      1. every enrolled fleet host exists as a Nagios host object
         (fleet-nagios-sync runs every minute, so this converges quickly), and
      2. every service from passive-checks.json on every one of those hosts has
         received at least one passive check result over NRDP (the agents
         submit once per interval, so allow at least one interval).

    Prints a per-host/per-service table of what is still missing while it
    waits, and exits non-zero if the estate has not converged when the time is
    up. A read-only (view_only) fleet API key is enough.
.PARAMETER FleetServer
    Fleet server url. Defaults to $env:NSCLIENT_FLEET_SERVER, then to the
    "Fleet Server" line of the .nagios.pwd file.
.PARAMETER ApiKey
    Fleet API key. Defaults to $env:NSCLIENT_FLEET_API_KEY, then
    $env:NSCLIENT_FLEET_SYNC_API_KEY.
.PARAMETER PwdFile
    The credentials file setup-nagios-machine.ps1 wrote (default:
    build/powershell/.nagios.pwd); supplies the Nagios address and password.
.PARAMETER WaitMinutes
    How long to keep polling before giving up (default 10). 0 checks once.
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate (self-signed test server).
.EXAMPLE
    ./verify-nagios-estate.ps1 -FleetServer https://fleet.example.com -WaitMinutes 15
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = "",
    [string]$PwdFile = "",
    [int]$WaitMinutes = 10,
    [switch]$FleetNoVerify
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")

if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".nagios.pwd" }
$nagios = Read-NagiosPwdFile -Path $PwdFile
if (-not $nagios.PublicIp -or -not $nagios.NagiosPassword) {
    Write-Error "❌ $PwdFile has no Nagios address/password - re-run setup-nagios-machine.ps1."
    exit 1
}
if (-not $FleetServer) { $FleetServer = $nagios.FleetServer }
if (-not $FleetServer) {
    Write-Error "❌ No fleet server: pass -FleetServer or set NSCLIENT_FLEET_SERVER."
    exit 1
}
if (-not $ApiKey) { $ApiKey = $env:NSCLIENT_FLEET_API_KEY }
if (-not $ApiKey) { $ApiKey = $env:NSCLIENT_FLEET_SYNC_API_KEY }
if (-not $ApiKey) {
    Write-Error "❌ No API key: pass -ApiKey or set NSCLIENT_FLEET_API_KEY (a view_only key is enough)."
    exit 1
}

$catalog = Read-PassiveCheckCatalog -Path (Join-Path $scriptDir "passive-checks.json")
$serviceNames = @($catalog.Services | ForEach-Object { $_.name })

$statusJsonBase = "http://$($nagios.PublicIp)/cgi-bin/nagios4/statusjson.cgi"
$user = $nagios.NagiosUser
if (-not $user) { $user = "nagiosadmin" }
$credential = New-Object System.Management.Automation.PSCredential(
    $user, (ConvertTo-SecureString -String $nagios.NagiosPassword -AsPlainText -Force))

# statusjson.cgi behind digest auth; -Credential negotiates that on both 5.1
# and 7+, but 7+ refuses to send credentials over plain http unless explicitly
# allowed. Every response carries result.type_text - "Success" or the error.
function Get-NagiosStatus {
    param([Parameter(Mandatory)][string]$Query)
    $params = @{
        Uri         = "${statusJsonBase}?$Query"
        Credential  = $credential
        TimeoutSec  = 30
        ErrorAction = "Stop"
    }
    if ($PSVersionTable.PSVersion.Major -ge 6) { $params.AllowUnencryptedAuthentication = $true }
    $response = Invoke-RestMethod @params
    if ($response.result.type_text -ne "Success") {
        throw "statusjson.cgi?$Query failed: $($response.result.message)"
    }
    return $response.data
}

Write-Host "Fleet server:  $FleetServer"
Write-Host "Nagios server: http://$($nagios.PublicIp)/nagios4/"
Write-Host "Services:      $($serviceNames -join ', ')"

$deadline = (Get-Date).AddMinutes($WaitMinutes)
while ($true) {
    $fleetHosts = Get-FleetHosts -FleetServer $FleetServer -ApiKey $ApiKey -EnrolledOnly -SkipCertificateCheck:$FleetNoVerify
    $expected = @($fleetHosts | ForEach-Object { $_.hostname } | Where-Object { $_ })
    if ($expected.Count -eq 0) {
        Write-Error "❌ The fleet server has no enrolled hosts - provision some first (provision-fleet-machines.ps1)."
        exit 1
    }

    # hostlist: { "<name>": <statuscode>, ... }. servicelist details: per
    # host/service objects with has_been_checked, status and plugin_output.
    $hostData = Get-NagiosStatus -Query "query=hostlist"
    $nagiosHosts = @($hostData.hostlist.PSObject.Properties | ForEach-Object { $_.Name })
    $serviceData = Get-NagiosStatus -Query "query=servicelist&details=true"

    $missingHosts = @()
    $waitingServices = @()
    foreach ($hostname in $expected) {
        if ($nagiosHosts -notcontains $hostname) {
            $missingHosts += $hostname
            continue
        }
        $hostServices = $serviceData.servicelist.PSObject.Properties |
        Where-Object { $_.Name -eq $hostname } | ForEach-Object { $_.Value }
        foreach ($serviceName in $serviceNames) {
            $service = $null
            if ($hostServices) {
                $service = $hostServices.PSObject.Properties |
                Where-Object { $_.Name -eq $serviceName } | ForEach-Object { $_.Value }
            }
            # has_been_checked alone is not enough: the freshness fallback is an
            # *active* run of fleet-stale that sets it too. Only a passive
            # result (check_type 1) proves the agent actually reported.
            if (-not $service -or -not $service.has_been_checked -or [int]$service.check_type -ne 1) {
                $waitingServices += "$hostname/$serviceName"
            }
        }
    }

    if ($missingHosts.Count -eq 0 -and $waitingServices.Count -eq 0) {
        Write-Host "✅ All $($expected.Count) fleet host(s) are registered in Nagios and every service has reported."
        foreach ($hostname in $expected) {
            $states = foreach ($serviceName in $serviceNames) {
                $service = $serviceData.servicelist.$hostname.$serviceName
                # statusjson service status codes (nagios statusdata.h):
                # 1 pending, 2 ok, 4 warning, 8 unknown, 16 critical.
                $word = switch ([int]$service.status) {
                    2 { "OK" } 4 { "WARNING" } 8 { "UNKNOWN" } 16 { "CRITICAL" } default { "PENDING" }
                }
                "${serviceName}=$word"
            }
            Write-Host ("   {0}: {1}" -f $hostname, ($states -join " "))
        }
        exit 0
    }

    $now = Get-Date
    if ($now -ge $deadline) {
        Write-Host ""
        Write-Error ("❌ The estate did not converge within $WaitMinutes minute(s).`n" +
            "   Hosts missing from Nagios: " + $(if ($missingHosts) { $missingHosts -join ", " } else { "none" }) + "`n" +
            "   Services without a result: " + $(if ($waitingServices) { $waitingServices -join ", " } else { "none" }) + "`n" +
            "   Check 'journalctl -u fleet-nagios-sync' on the Nagios VM and the agents' nsclient.log.")
        exit 1
    }
    Write-Host ("[{0:HH:mm:ss}] waiting - hosts missing: {1}, services without a result: {2} (until {3:HH:mm:ss})" -f `
            $now, $missingHosts.Count, $waitingServices.Count, $deadline)
    Start-Sleep -Seconds 30
}
