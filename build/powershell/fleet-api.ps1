<#
.SYNOPSIS
    Helper functions for talking to an NSClient fleet server, and for inventing
    plausible machine names. Dot-source it; it defines functions and does nothing
    on its own:

        . "$PSScriptRoot/fleet-api.ps1"
        $h = New-FleetHost -FleetServer https://fleet.example.com -ApiKey $env:NSCLIENT_FLEET_API_KEY
        nscp enroll --server https://fleet.example.com --token $h.BootstrapToken

.DESCRIPTION
    The fleet server mints one host at a time: POST /api/hosts with an API key
    ("nsk_...", minted from *API keys* in the fleet UI) returns a host id and a
    one-time bootstrap token that the agent exchanges for its client certificate.
    The token expires (an hour by default) and is burned on first use, so mint it
    immediately before installing the agent rather than up front for a batch.

    Everything here works on both Windows PowerShell 5.1 and PowerShell 7+, which
    differ in how they skip certificate validation and where they put the body of
    an HTTP error response.
#>

# Ignore an untrusted/self-signed fleet certificate for the rest of this process.
# PowerShell 7 takes -SkipCertificateCheck per call; 5.1 has no such parameter, so
# the only lever is the process-wide validation callback. Both are one-way for the
# life of the process - this is test tooling, not something to load into a shell
# you then use for anything else.
function Set-FleetCertificateCheck {
    param([bool]$Skip)
    if (-not $Skip) { return }
    if ($PSVersionTable.PSVersion.Major -lt 6) {
        # 5.1 also still defaults to TLS 1.0 on older hosts, which every current
        # server refuses; opt into 1.2 while we are here.
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
        [System.Net.ServicePointManager]::ServerCertificateValidationCallback = { $true }
    }
}

# The body of a failed request: the fleet server explains itself there ("invalid
# token", the tier_limit JSON, ...), and the bare exception message does not.
function Get-FleetErrorBody {
    param($ErrorRecord)
    if ($ErrorRecord.ErrorDetails -and $ErrorRecord.ErrorDetails.Message) {
        return $ErrorRecord.ErrorDetails.Message
    }
    $response = $ErrorRecord.Exception.Response
    # 5.1 hands back an HttpWebResponse whose body must be read from the stream;
    # 7's HttpResponseMessage has no GetResponseStream at all.
    if ($response -and ($response | Get-Member -Name GetResponseStream -MemberType Method)) {
        try {
            $reader = New-Object System.IO.StreamReader($response.GetResponseStream())
            try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
        }
        catch { }
    }
    return $ErrorRecord.Exception.Message
}

function Invoke-FleetApi {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$Path,
        [string]$Method = "Get",
        [string]$ApiKey,
        # A hashtable/object to send as the JSON request body.
        [object]$Body = $null,
        [switch]$SkipCertificateCheck,
        [int]$TimeoutSec = 30
    )
    Set-FleetCertificateCheck -Skip ([bool]$SkipCertificateCheck)

    $uri = "$($FleetServer.TrimEnd('/'))$Path"
    $params = @{
        Uri         = $uri
        Method      = $Method
        TimeoutSec  = $TimeoutSec
        ErrorAction = "Stop"
    }
    if ($ApiKey) { $params.Headers = @{ Authorization = "Bearer $ApiKey" } }
    if ($null -ne $Body) {
        # -Depth: the default (2) silently truncates nested structures like a
        # bundle's config_json; 5.1 also needs the charset spelled out or
        # non-ASCII ends up mangled on the wire.
        $params.Body = [System.Text.Encoding]::UTF8.GetBytes(($Body | ConvertTo-Json -Depth 16))
        $params.ContentType = "application/json; charset=utf-8"
    }
    if ($SkipCertificateCheck -and $PSVersionTable.PSVersion.Major -ge 6) {
        $params.SkipCertificateCheck = $true
    }

    try {
        return Invoke-RestMethod @params
    }
    catch {
        $status = $null
        if ($_.Exception.Response) { $status = [int]$_.Exception.Response.StatusCode }
        $body = (Get-FleetErrorBody $_).Trim()
        $hint = switch ($status) {
            401 { " (the API key is unknown, revoked, or its owner was deleted)" }
            403 { " (the key's owner may lack the add_hosts right, or the tenant is at its host limit)" }
            404 { " (is '$FleetServer' really a fleet server? check the url and its path prefix)" }
            429 { " (rate limited - wait and retry)" }
            default { "" }
        }
        $code = if ($status) { "HTTP $status" } else { "request failed" }
        throw "$Method $uri : $code$hint`n$body"
    }
}

# Fail fast, before spending ten minutes creating a VM that then cannot enroll.
# Note this only proves the fleet server answers *this* machine - the VM has to
# reach the same url from Azure, so a localhost/LAN url will still fail later.
function Test-FleetServer {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [switch]$SkipCertificateCheck
    )
    $health = "$(Invoke-FleetApi -FleetServer $FleetServer -Path "/healthz" -SkipCertificateCheck:$SkipCertificateCheck -TimeoutSec 15)".Trim()
    # Anything that is not a fleet server answers /healthz with a 404 page or its
    # own index.html - and the server returns its SPA for unknown paths, so a
    # wrong url reaches us as a wall of HTML rather than as an error.
    if ($health -ne "OK") {
        $sample = if ($health.Length -gt 80) { $health.Substring(0, 80) + "..." } else { $health }
        throw "$($FleetServer.TrimEnd('/'))/healthz did not answer 'OK' - is that a fleet server url? It said: $sample"
    }
    return $health
}

<#
.SYNOPSIS
    Create a pending host on the fleet server and return its one-time bootstrap
    token.
.OUTPUTS
    An object with HostId, BootstrapToken, InstallCommand and ExpiresAt (a local
    DateTime; the API returns unix seconds).
#>
function New-FleetHost {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [switch]$SkipCertificateCheck
    )
    $response = Invoke-FleetApi -FleetServer $FleetServer -Path "/api/hosts" -Method Post `
        -ApiKey $ApiKey -SkipCertificateCheck:$SkipCertificateCheck

    if (-not $response.bootstrap_token) {
        throw "The fleet server did not return a bootstrap token for POST /api/hosts."
    }
    [pscustomobject]@{
        HostId         = $response.host_id
        BootstrapToken = $response.bootstrap_token
        InstallCommand = $response.install_command
        ExpiresAt      = [DateTimeOffset]::FromUnixTimeSeconds([int64]$response.expires_at).LocalDateTime
    }
}

<#
.SYNOPSIS
    Delete a host from the fleet server, so tearing down the VM does not leave a
    permanently offline machine in the fleet.
.DESCRIPTION
    Unlike creating a host this needs configuration rights (owner/admin): an
    API key belonging to an add_hosts user can enroll machines but not remove
    them, and gets a 403 here.
#>
function Remove-FleetHost {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [Parameter(Mandatory)][string]$HostId,
        [switch]$SkipCertificateCheck
    )
    Invoke-FleetApi -FleetServer $FleetServer -Path "/api/hosts/$HostId" -Method Delete `
        -ApiKey $ApiKey -SkipCertificateCheck:$SkipCertificateCheck | Out-Null
}

<#
.SYNOPSIS
    List the hosts the fleet server knows about (GET /api/hosts).
.DESCRIPTION
    Returns the raw host objects (id, hostname, os, status, last_seen_at, ...).
    A view_only API key is enough. -EnrolledOnly keeps just the hosts an agent
    has actually joined from (status in_sync/out_of_sync/offline/lost),
    which is the set a monitoring server should know about.
#>
function Get-FleetHosts {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [switch]$EnrolledOnly,
        [switch]$SkipCertificateCheck
    )
    $hosts = @(Invoke-FleetApi -FleetServer $FleetServer -Path "/api/hosts" `
            -ApiKey $ApiKey -SkipCertificateCheck:$SkipCertificateCheck)
    if ($EnrolledOnly) {
        # enrolled_at is how the server itself defines "an agent joined";
        # unlike the status vocabulary it cannot drift.
        $hosts = @($hosts | Where-Object { $null -ne $_.enrolled_at -and $_.hostname })
    }
    return $hosts
}

<#
.SYNOPSIS
    Read and validate nagios/passive-checks.json, the service catalog shared by
    every stage of the Nagios turn-key flow.
.OUTPUTS
    An object with Services (name/command objects), Interval ("300s") and
    IntervalSeconds. Throws on anything the carriers could not represent: the
    names become scheduler section names, NRDP service names, Nagios
    service_descriptions and lines in a plain text list.
#>
function Read-PassiveCheckCatalog {
    param([Parameter(Mandatory)][string]$Path)
    $catalog = Get-Content -Path $Path -Raw | ConvertFrom-Json
    $services = @($catalog.services)
    if ($services.Count -eq 0) { throw "$Path has no services." }
    foreach ($service in $services) {
        if ("$($service.name)" -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
            throw "$Path : service name '$($service.name)' is not a plain identifier (letters, digits, '.', '_', '-')."
        }
        if (-not "$($service.command)") { throw "$Path : service '$($service.name)' has no command." }
    }
    if ("$($catalog.interval)" -notmatch '^(\d+)s$') {
        throw "$Path : interval '$($catalog.interval)' must be a number of seconds like '300s'."
    }
    [pscustomobject]@{
        Services        = $services
        Interval        = "$($catalog.interval)"
        IntervalSeconds = [int]$Matches[1]
    }
}

<#
.SYNOPSIS
    Parse the .nagios.pwd file written by setup-nagios-machine.ps1.
.OUTPUTS
    An object with PublicIp, NagiosUrl, NagiosUser, NagiosPassword, NrdpUrl,
    NrdpToken and FleetServer (missing lines come back empty).
#>
function Read-NagiosPwdFile {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path $Path)) {
        throw "Nagios credentials file '$Path' does not exist - run setup-nagios-machine.ps1 first, or pass the values explicitly."
    }
    $values = @{}
    foreach ($line in Get-Content -Path $Path) {
        if ($line -match '^([^:]+):\s*(.*)$') {
            $values[$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }
    [pscustomobject]@{
        PublicIp       = "$($values['Public IP'])"
        NagiosUrl      = "$($values['Nagios URL'])"
        NagiosUser     = "$($values['Nagios User'])"
        NagiosPassword = "$($values['Nagios Password'])"
        NrdpUrl        = "$($values['NRDP URL'])"
        NrdpToken      = "$($values['NRDP Token'])"
        FleetServer    = "$($values['Fleet Server'])"
    }
}

# Machine names that look like something out of a real estate rather than
# "vm-test-3": <role>-<site>-<nn>, e.g. web-ams-04, sql-fra-12. Kept lowercase,
# alphanumeric-and-hyphen, and at most 12 characters, so the same name is legal
# as an Azure resource name, a Linux hostname, and a Windows computer name (which
# is capped at 15).
$script:FleetNameRoles = @(
    "web", "app", "api", "sql", "db", "dc", "file", "mail", "print", "proxy",
    "cache", "mon", "log", "bkp", "vpn", "dns", "git", "ci", "erp", "term"
)
$script:FleetNameSites = @(
    "ams", "fra", "lon", "sto", "par", "mad", "osl", "cph", "dub", "hel",
    "nyc", "chi", "sfo", "tor", "syd", "sgp", "tky", "bos", "atl", "sea"
)

function New-FleetMachineName {
    param(
        # Names already taken (this run's names, plus anything else you want to
        # avoid). Comparison is case-insensitive.
        [string[]]$Exclude = @(),
        [int]$MaxAttempts = 200
    )
    $taken = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($e in $Exclude) { if ($e) { [void]$taken.Add($e) } }

    for ($i = 0; $i -lt $MaxAttempts; $i++) {
        $name = "{0}-{1}-{2:d2}" -f (Get-Random -InputObject $script:FleetNameRoles),
        (Get-Random -InputObject $script:FleetNameSites),
        (Get-Random -Minimum 1 -Maximum 20)
        if (-not $taken.Contains($name)) { return $name }
    }
    # 20*20*19 combinations, so this only happens if a caller asks for an absurd
    # number of machines - fall back to a suffix rather than looping forever.
    return ("{0}-{1}-{2:d2}x{3}" -f (Get-Random -InputObject $script:FleetNameRoles),
        (Get-Random -InputObject $script:FleetNameSites),
        (Get-Random -Minimum 1 -Maximum 20), (Get-Random -Minimum 100 -Maximum 999))
}
