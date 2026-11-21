<#
.SYNOPSIS
    Verify the turn-key Icinga flow end to end: every enrolled fleet host has
    created itself in Icinga 2 and its passive checks have delivered results.
.DESCRIPTION
    Stage 4 of the turn-key Icinga flow (see README.md). Reads the enrolled
    hosts from the fleet server (GET /api/hosts) and the monitoring state from
    the Icinga VM's REST API (/v1/objects/hosts and /v1/objects/services), then
    waits (up to -WaitMinutes) until, for every machine in the estate manifest
    (or, without one, every enrolled fleet host),

      1. a Host object of that name exists - the agent's IcingaClient created
         it (`ensure objects`), nothing else does;
      2. the host itself has a passive result, when the catalog has a
         "host_check" entry; and
      3. every other entry of passive-checks.json exists as a Service of that
         host whose last result is a passive one.

    "Passive" is last_check_result.active == false. A result alone proves
    nothing: the templates' freshness check runs the dummy command when an
    agent is silent, and that leaves a last_check_result too.

    Prints what is still missing while it waits, and exits non-zero if the
    estate has not converged when the time is up. A read-only (view_only)
    fleet API key is enough.
.PARAMETER FleetServer
    Fleet server url. Defaults to $env:NSCLIENT_FLEET_SERVER, then to the
    "Fleet Server" line of the .icinga.pwd file.
.PARAMETER ApiKey
    Fleet API key. Defaults to $env:NSCLIENT_FLEET_API_KEY, then
    $env:NSCLIENT_FLEET_SYNC_API_KEY.
.PARAMETER PwdFile
    The credentials file setup-icinga-machine.ps1 wrote (default:
    build/powershell/.icinga.pwd); supplies the Icinga address and the admin
    API user.
.PARAMETER Hosts
    Only these fleet hosts (by hostname). Overrides the manifest.
.PARAMETER ManifestFile
    The estate manifest provision-fleet-machines.ps1 wrote (default:
    build/powershell/.fleet-machines.json). When it lists machines, those are
    the hosts the gate requires - other hosts enrolled with the same fleet
    server are ignored. A manifest machine that has not enrolled yet is waited
    for. Without a manifest every enrolled host is required.
.PARAMETER AllHosts
    Ignore the manifest and require every host enrolled with the fleet server.
.PARAMETER Mode
    "passive" (the default) is the flow described above. "agent" gates the
    agent-mode flow instead: every host must exist and be UP (its agent is
    connected), and every entry of agent-checks.json must be a Service of it
    whose last result came from the agent itself (check_source is the agent,
    not the master answering for an agent that is not connected).
.PARAMETER WaitMinutes
    How long to keep polling before giving up (default 10). 0 checks once.
.PARAMETER FleetNoVerify
    Skip verification of the fleet server certificate (self-signed test server).
.EXAMPLE
    ./verify-icinga-estate.ps1 -FleetServer https://fleet.example.com -WaitMinutes 15
#>
param(
    [string]$FleetServer = $env:NSCLIENT_FLEET_SERVER,
    [string]$ApiKey = "",
    [string]$PwdFile = "",
    [string[]]$Hosts = @(),
    [string]$ManifestFile = "",
    [switch]$AllHosts,
    [ValidateSet("passive", "agent")][string]$Mode = "passive",
    [int]$WaitMinutes = 10,
    [switch]$FleetNoVerify
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$parentDir = Split-Path -Parent $scriptDir

. (Join-Path $parentDir "fleet-api.ps1")
Import-FleetConfig
if (-not $FleetServer) { $FleetServer = $env:NSCLIENT_FLEET_SERVER }

if (-not $PwdFile) { $PwdFile = Join-Path $parentDir ".icinga.pwd" }
$icinga = Read-IcingaPwdFile -Path $PwdFile
if (-not $icinga.IcingaUrl -or -not $icinga.AdminUser -or -not $icinga.AdminPassword) {
    Write-Error "❌ $PwdFile has no Icinga url/admin user - re-run setup-icinga-machine.ps1."
    exit 1
}
if (-not $FleetServer) { $FleetServer = $icinga.FleetServer }
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

$catalog = if ($Mode -eq "agent") { Read-AgentCheckCatalog -Path (Join-Path $scriptDir "agent-checks.json") }
else { Read-PassiveCheckCatalog -Path (Join-Path $scriptDir "passive-checks.json") }
# IcingaClient submits the schedule called host_check as the host's own result.
# In agent mode the host has a check of its own (cluster-zone), required below.
$expectHostCheck = $Mode -eq "agent" -or [bool]($catalog.Services | Where-Object { $_.name -eq "host_check" })
$serviceNames = @($catalog.Services | ForEach-Object { $_.name } | Where-Object { $_ -ne "host_check" })

$icingaBase = $icinga.IcingaUrl.TrimEnd('/')
$icingaHost = ([uri]$icingaBase).Host
$icingaAuth = "Basic " + [Convert]::ToBase64String(
    [System.Text.Encoding]::UTF8.GetBytes("$($icinga.AdminUser):$($icinga.AdminPassword)"))

# The Icinga VM's certificate is issued by its own CA, for its node name, so
# it cannot be verified from here. Accept it for that one host only: 7+ takes
# -SkipCertificateCheck per call, but on 5.1 the only lever is the process-wide
# validation callback - and a blanket "accept everything" there would also
# stop this script verifying the fleet server it sends an API key to.
if ($PSVersionTable.PSVersion.Major -lt 6) {
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
    if (-not ("IcingaTrustOneHost" -as [type])) {
        # Compiled, not a scriptblock: .NET may run the validation on a thread
        # with no runspace (see Set-FleetCertificateCheck).
        Add-Type -TypeDefinition @"
using System.Net;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
public static class IcingaTrustOneHost {
    public static string Host = "";
    public static bool Accept(object sender, X509Certificate certificate, X509Chain chain, SslPolicyErrors errors) {
        if (errors == SslPolicyErrors.None) { return true; }
        HttpWebRequest request = sender as HttpWebRequest;
        return request != null && Host.Length > 0 &&
            string.Equals(request.RequestUri.Host, Host, System.StringComparison.OrdinalIgnoreCase);
    }
}
"@
    }
    # (-FleetNoVerify swaps in an accept-everything callback in Invoke-FleetApi.)
    [IcingaTrustOneHost]::Host = $icingaHost
    [System.Net.ServicePointManager]::ServerCertificateValidationCallback =
    [System.Delegate]::CreateDelegate([System.Net.Security.RemoteCertificateValidationCallback], [IcingaTrustOneHost], "Accept")
}

# GET an object list from the Icinga 2 API; returns the `results` array. Icinga
# answers without an Accept header with a 400, hence the header.
function Get-IcingaObjects {
    param([Parameter(Mandatory)][string]$Path)
    $params = @{
        Uri         = "$icingaBase$Path"
        Headers     = @{ Authorization = $icingaAuth; Accept = "application/json" }
        TimeoutSec  = 30
        ErrorAction = "Stop"
    }
    if ($PSVersionTable.PSVersion.Major -ge 6) { $params.SkipCertificateCheck = $true }
    try { $response = Invoke-RestMethod @params }
    catch { throw "GET $icingaBase$Path failed: $($_.Exception.Message)" }
    return @($response.results | ForEach-Object { $_ })
}

# Only a result an agent submitted counts (see the description).
function Test-PassiveResult {
    param($Object)
    if (-not $Object) { return $false }
    $result = $Object.attrs.last_check_result
    return ($null -ne $result -and $result.active -eq $false)
}

# Agent mode: a result only counts when the agent itself produced it. With
# `command_endpoint` the result's check_source is the agent's endpoint name;
# while the agent is not connected Icinga answers in its place (UNKNOWN,
# check_source = the master), and that must not pass.
function Test-AgentResult {
    param($Object, [string]$Endpoint)
    if (-not $Object) { return $false }
    $result = $Object.attrs.last_check_result
    return ($null -ne $result -and "$($result.check_source)" -eq $Endpoint)
}

# The host is checked by the master (cluster-zone): UP means the agent's zone
# is connected.
function Test-AgentHost {
    param($Object)
    if (-not $Object) { return $false }
    return ($null -ne $Object.attrs.last_check_result -and [int]$Object.attrs.state -eq 0)
}

# Which hosts: -Hosts, else the estate manifest, else (or with -AllHosts)
# everything enrolled. The fleet server usually holds machines this estate does
# not own - a developer's own desktop, say - and one of those that is switched
# off would fail the gate for ever.
$hostsFromManifest = $false
if (-not $ManifestFile) { $ManifestFile = Join-Path $parentDir ".fleet-machines.json" }
if ($Hosts.Count -eq 0 -and -not $AllHosts) {
    $Hosts = @(Read-FleetManifest -Path $ManifestFile | ForEach-Object { "$($_.Name)" } | Where-Object { $_ })
    $hostsFromManifest = $Hosts.Count -gt 0
}

if ($Mode -eq "agent") {
    $resultKind = "a result from the agent"
    $hint = "   A missing host was never registered: the sync timer on the Icinga VM adds a host within a minute of its`n" +
    "   enrollment (journalctl -u fleet-icinga-agent-sync), and skips one that is already defined elsewhere.`n" +
    "   A check without a result from the agent means the agent is not connected: check its nsclient.log for`n" +
    "   'Icinga agent' lines, that the build has the IcingaAgent module and it was loaded when the bundle arrived,`n" +
    "   and that the machine can reach port 5665 on the Icinga VM."
}
else {
    $resultKind = "a passive result"
    $hint = "   A missing host never submitted anything: check the agent's nsclient.log for IcingaClient errors,`n" +
    "   that IcingaClient and Scheduler were loaded when the bundle arrived (they are activated at install`n" +
    "   time by the setup scripts), and that the machine can reach port 5665 on the Icinga VM."
}

Write-Host "Fleet server:  $FleetServer"
Write-Host "Mode:          $Mode"
Write-Host ("Hosts:         " + $(if ($hostsFromManifest) { "the $($Hosts.Count) machine(s) in $ManifestFile" }
        elseif ($Hosts.Count -gt 0) { $Hosts -join ', ' }
        else { "every host enrolled with the fleet server" }))
Write-Host "Icinga server: $icingaBase/v1"
Write-Host ("Checks:        " + (@($catalog.Services | ForEach-Object { $_.name }) -join ', '))

$notEnrolled = @()
$deadline = (Get-Date).AddMinutes($WaitMinutes)
while ($true) {
    $fleetHosts = Get-FleetHosts -FleetServer $FleetServer -ApiKey $ApiKey -EnrolledOnly -SkipCertificateCheck:$FleetNoVerify
    $expected = @($fleetHosts | ForEach-Object { $_.hostname } | Where-Object { $_ })
    if ($Hosts.Count -gt 0) {
        $unknown = @($Hosts | Where-Object { $expected -notcontains $_ })
        # A name typed on the command line that the fleet server does not know
        # is a typo; a manifest machine it does not know may still be enrolling,
        # so that one is waited for (it counts as missing from Icinga).
        if ($unknown.Count -gt 0 -and -not $hostsFromManifest) {
            Write-Error "❌ Not enrolled with the fleet server: $($unknown -join ', ')."
            exit 1
        }
        $notEnrolled = $unknown
        $expected = @($expected | Where-Object { $Hosts -contains $_ }) + $unknown
    }
    if ($expected.Count -eq 0) {
        Write-Error "❌ The fleet server has no enrolled hosts - provision some first (provision-fleet-machines.ps1)."
        exit 1
    }

    # Object names are case-sensitive in Icinga and the agent names its host
    # after its own OS host name; look both up case-insensitively, so a fleet
    # record that differs only in case does not read as "never reported".
    $hostObjects = @{}
    foreach ($object in Get-IcingaObjects -Path "/v1/objects/hosts?attrs=name&attrs=state&attrs=last_check_result") {
        $hostObjects["$($object.attrs.name)".ToLowerInvariant()] = $object
    }
    $serviceObjects = @{}
    foreach ($object in Get-IcingaObjects -Path "/v1/objects/services?attrs=name&attrs=host_name&attrs=state&attrs=last_check_result") {
        $serviceObjects["$($object.attrs.host_name)!$($object.attrs.name)".ToLowerInvariant()] = $object
    }

    $missingHosts = @()
    $waiting = @()
    foreach ($hostname in $expected) {
        $key = $hostname.ToLowerInvariant()
        if (-not $hostObjects.ContainsKey($key)) {
            $missingHosts += $hostname
            continue
        }
        $hostOk = if ($Mode -eq "agent") { Test-AgentHost $hostObjects[$key] } else { Test-PassiveResult $hostObjects[$key] }
        if ($expectHostCheck -and -not $hostOk) {
            $waiting += "$hostname/host_check"
        }
        foreach ($serviceName in $serviceNames) {
            $service = $serviceObjects["$key!$serviceName".ToLowerInvariant()]
            # $key is also the agent's endpoint name: its lower-cased host name.
            $serviceOk = if ($Mode -eq "agent") { Test-AgentResult $service $key } else { Test-PassiveResult $service }
            if (-not $serviceOk) {
                $waiting += "$hostname/$serviceName"
            }
        }
    }

    if ($missingHosts.Count -eq 0 -and $waiting.Count -eq 0) {
        Write-Host "✅ All $($expected.Count) fleet host(s) exist in Icinga and every check has reported."
        foreach ($hostname in $expected) {
            $key = $hostname.ToLowerInvariant()
            $states = @()
            # Icinga 2 states: a host is 0 UP / 1 DOWN, a service 0..3.
            $hostWord = if ([int]$hostObjects[$key].attrs.state -eq 0) { "UP" } else { "DOWN" }
            $states += "host=$hostWord"
            foreach ($serviceName in $serviceNames) {
                $word = switch ([int]$serviceObjects["$key!$serviceName".ToLowerInvariant()].attrs.state) {
                    0 { "OK" } 1 { "WARNING" } 2 { "CRITICAL" } default { "UNKNOWN" }
                }
                $states += "${serviceName}=$word"
            }
            Write-Host ("   {0}: {1}" -f $hostname, ($states -join " "))
        }
        exit 0
    }

    $now = Get-Date
    if ($now -ge $deadline) {
        Write-Host ""
        Write-Error ("❌ The estate did not converge within $WaitMinutes minute(s).`n" +
            "   Hosts missing from Icinga:      " + $(if ($missingHosts) { $missingHosts -join ", " } else { "none" }) + "`n" +
            "   Checks without ${resultKind}: " + $(if ($waiting) { $waiting -join ", " } else { "none" }) + "`n" +
            $(if ($notEnrolled) { "   Not enrolled with the fleet server: " + ($notEnrolled -join ", ") + " (in the manifest, but the fleet server has no such host)`n" } else { "" }) +
            $hint)
        exit 1
    }
    Write-Host ("[{0:HH:mm:ss}] waiting - hosts missing: {1}, checks without {4}: {2} (until {3:HH:mm:ss})" -f `
            $now, $missingHosts.Count, $waiting.Count, $deadline, $resultKind)
    Start-Sleep -Seconds 30
}
