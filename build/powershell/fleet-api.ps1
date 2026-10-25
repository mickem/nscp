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

# Fill the NSCLIENT_FLEET_* variables from a local config file, so the fleet
# server url and its API keys need not be set in every shell:
#
#     build/powershell/.fleet-config.json      (gitignored)
#     { "server": "https://fleet.example.com", "apiKey": "nsk_...", "syncApiKey": "nsk_..." }
#
# Copy fleet-config.example.json to start one; NSCLIENT_FLEET_CONFIG points at
# a file elsewhere. A variable that is already set wins over the file (and an
# explicit parameter over both, in the scripts that call this). The values go
# into the process environment, which is how the runners already hand the key
# to their child processes - never on a command line.
function Import-FleetConfig {
    param([string]$Path = $env:NSCLIENT_FLEET_CONFIG)
    if (-not $Path) { $Path = Join-Path $PSScriptRoot ".fleet-config.json" }
    if (-not (Test-Path -LiteralPath $Path)) { return }
    try {
        $config = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    }
    catch {
        throw "Could not read the fleet config '$Path' - it must be JSON shaped like fleet-config.example.json: $($_.Exception.Message)"
    }
    $variables = [ordered]@{
        server     = "NSCLIENT_FLEET_SERVER"
        apiKey     = "NSCLIENT_FLEET_API_KEY"
        syncApiKey = "NSCLIENT_FLEET_SYNC_API_KEY"
    }
    foreach ($name in $variables.Keys) {
        $value = "$($config.$name)"
        if ($value -and -not [Environment]::GetEnvironmentVariable($variables[$name])) {
            [Environment]::SetEnvironmentVariable($variables[$name], $value)
        }
    }
}

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
        # A compiled callback, not a scriptblock: .NET may run the validation
        # on a thread-pool thread with no runspace, where a scriptblock throws
        # "There is no Runspace available" and the handshake fails.
        if (-not ("FleetTrustAllCertificates" -as [type])) {
            Add-Type -TypeDefinition @"
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
public static class FleetTrustAllCertificates {
    public static bool Accept(object sender, X509Certificate certificate, X509Chain chain, SslPolicyErrors errors) { return true; }
}
"@
        }
        [System.Net.ServicePointManager]::ServerCertificateValidationCallback =
        [System.Delegate]::CreateDelegate([System.Net.Security.RemoteCertificateValidationCallback], [FleetTrustAllCertificates], "Accept")
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
        $response = Invoke-RestMethod @params
    }
    catch {
        $status = $null
        if ($_.Exception.Response) { $status = [int]$_.Exception.Response.StatusCode }
        $body = (Get-FleetErrorBody $_).Trim()
        $hint = switch ($status) {
            401 { " (the API key is unknown, revoked, or its owner was deleted)" }
            403 { " (the key's owner lacks the right for this call, or the tenant is at a tier limit - hosts, bundles, ...; the body below says which)" }
            404 { " (is '$FleetServer' really a fleet server? check the url and its path prefix)" }
            429 { " (rate limited - wait and retry)" }
            default { "" }
        }
        $code = if ($status) { "HTTP $status" } else { "request failed" }
        throw "$Method $uri : $code$hint`n$body"
    }
    # A JSON array comes back as ONE pipeline object (an Object[]), and
    # `return` would hand it on as such: a caller's @(...) then holds a single
    # element that is the whole list, `Where-Object { $_.name -eq $x }` passes
    # that list through as a whole, and `$group.id` is every id at once. Pipe
    # it so the elements come out one by one, as the callers expect.
    return $response | ForEach-Object { $_ }
}

<#
.SYNOPSIS
    Upload a bundle zip (POST /api/bundles, multipart): the path for a bundle
    that carries scripts, which the compose endpoint cannot build.
.DESCRIPTION
    Returns the bundle the server stored (id, name, version), as compose
    does. Goes through HttpClient rather than Invoke-RestMethod: 5.1 has no
    -Form, and building the multipart body by hand is what the two versions
    least agree on.
#>
function Invoke-FleetBundleUpload {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][byte[]]$Bundle,
        [switch]$SkipCertificateCheck,
        [int]$TimeoutSec = 120
    )
    Set-FleetCertificateCheck -Skip ([bool]$SkipCertificateCheck)
    Add-Type -AssemblyName System.Net.Http
    $handler = New-Object System.Net.Http.HttpClientHandler
    if ($SkipCertificateCheck -and $PSVersionTable.PSVersion.Major -ge 6) {
        $handler.ServerCertificateCustomValidationCallback = [System.Net.Http.HttpClientHandler]::DangerousAcceptAnyServerCertificateValidator
    }
    $client = New-Object System.Net.Http.HttpClient($handler)
    $form = New-Object System.Net.Http.MultipartFormDataContent
    $uri = "$($FleetServer.TrimEnd('/'))/api/bundles"
    try {
        $client.Timeout = [TimeSpan]::FromSeconds($TimeoutSec)
        $client.DefaultRequestHeaders.Authorization = New-Object System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", $ApiKey)
        $form.Add((New-Object System.Net.Http.StringContent($Name)), "name")
        $form.Add((New-Object System.Net.Http.StringContent($Version)), "version")
        $form.Add((New-Object System.Net.Http.StringContent("plain")), "format")
        $file = New-Object System.Net.Http.ByteArrayContent(, $Bundle)
        $file.Headers.ContentType = [System.Net.Http.Headers.MediaTypeHeaderValue]::Parse("application/zip")
        $form.Add($file, "bundle", "$Name-$Version.zip")
        $response = $client.PostAsync($uri, $form).GetAwaiter().GetResult()
        $text = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        if (-not $response.IsSuccessStatusCode) {
            $status = [int]$response.StatusCode
            $hint = switch ($status) {
                401 { " (the API key is unknown, revoked, or its owner was deleted)" }
                403 { " (the key's owner lacks the right for this call, or the tenant is at a tier limit; the body below says which)" }
                default { "" }
            }
            throw "POST $uri : HTTP $status$hint`n$text"
        }
        return $text | ConvertFrom-Json
    }
    finally {
        $form.Dispose()
        $client.Dispose()
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
    The tag of the latest NSClient++ release on GitHub ("0.24.1"), which is what
    the provisioning scripts install when no -Version is given.
.DESCRIPTION
    Asks api.github.com for releases/latest: a published, non-prerelease,
    non-draft release, the same one github.com/mickem/nscp/releases/latest
    redirects to. Unauthenticated calls are rate-limited per address (60/hour);
    set GITHUB_TOKEN to lift that. Throws, with the reason, when the API cannot
    be reached, so a run says so before it creates anything.
#>
function Get-LatestNscpRelease {
    param([string]$Repository = "mickem/nscp")
    $params = @{
        Uri         = "https://api.github.com/repos/$Repository/releases/latest"
        Headers     = @{ Accept = "application/vnd.github+json"; "User-Agent" = "nscp-fleet-tools" }
        TimeoutSec  = 20
        ErrorAction = "Stop"
    }
    if ($env:GITHUB_TOKEN) { $params.Headers.Authorization = "Bearer $env:GITHUB_TOKEN" }
    try { $release = Invoke-RestMethod @params }
    catch { throw "Could not look up the latest NSClient++ release ($($params.Uri)): $($_.Exception.Message). Pass -Version explicitly." }
    $tag = "$($release.tag_name)".Trim()
    if (-not $tag) { throw "GitHub returned no tag_name for the latest $Repository release; pass -Version explicitly." }
    return $tag
}

# Prove the key works before creating anything: a 401 on the first host create
# otherwise surfaces after the Azure login and the capacity preflight, and in
# an estate run once per suite. GET /api/hosts needs no more than view_only.
function Test-FleetApiKey {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [switch]$SkipCertificateCheck
    )
    try {
        Invoke-FleetApi -FleetServer $FleetServer -Path "/api/hosts" -ApiKey $ApiKey `
            -SkipCertificateCheck:$SkipCertificateCheck -TimeoutSec 15 | Out-Null
    }
    catch {
        throw "The fleet API key does not work against $($FleetServer.TrimEnd('/')) - mint one from *API keys* in the fleet UI and set `$env:NSCLIENT_FLEET_API_KEY. $($_.Exception.Message)"
    }
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
    Set an operator-sourced tag on a host (PUT /api/hosts/<id>/tags/<key>).
.DESCRIPTION
    Group selectors match operator-set tags by default, so this is how a
    machine is placed in a group before it ever syncs: tag it right after
    New-FleetHost and its first desired state already carries the group's
    bundles. Needs configuration rights, like Remove-FleetHost.
#>
function Set-FleetHostTag {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [Parameter(Mandatory)][string]$HostId,
        [Parameter(Mandatory)][string]$Key,
        [Parameter(Mandatory)][string]$Value,
        [switch]$SkipCertificateCheck
    )
    Invoke-FleetApi -FleetServer $FleetServer -Path "/api/hosts/$HostId/tags/$([uri]::EscapeDataString($Key))" -Method Put `
        -ApiKey $ApiKey -Body @{ value = $Value } -SkipCertificateCheck:$SkipCertificateCheck | Out-Null
}

<#
.SYNOPSIS
    Delete every stored version of a bundle except the ones in -Keep.
.DESCRIPTION
    Every compose stores a new, immutable version, and a tenant may only hold
    so many bundles (50 on the smallest tier) - so a script that composes on
    every run has to prune too, or it eventually fails with "tier_limit".
    Deleting a bundle also removes its group assignments, so keep whatever is
    still meant to be assigned. Needs configuration rights (owner/admin).
.OUTPUTS
    One line per deleted version, for the caller to print.
#>
function Remove-FleetBundleVersions {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [Parameter(Mandatory)][string]$Name,
        [string[]]$Keep = @(),
        [switch]$SkipCertificateCheck
    )
    $bundles = @(Invoke-FleetApi -FleetServer $FleetServer -Path "/api/bundles" -ApiKey $ApiKey `
            -SkipCertificateCheck:$SkipCertificateCheck)
    foreach ($b in @($bundles | Where-Object { $_.name -eq $Name -and $Keep -notcontains $_.id })) {
        Invoke-FleetApi -FleetServer $FleetServer -Path "/api/bundles/$($b.id)" -Method Delete -ApiKey $ApiKey `
            -SkipCertificateCheck:$SkipCertificateCheck | Out-Null
        "removed $Name $($b.version)"
    }
}

<#
.SYNOPSIS
    Read one host (GET /api/hosts/<id>): status, enrolled_at, last_seen_at, ...
#>
function Get-FleetHost {
    param(
        [Parameter(Mandatory)][string]$FleetServer,
        [Parameter(Mandatory)][string]$ApiKey,
        [Parameter(Mandatory)][string]$HostId,
        [switch]$SkipCertificateCheck
    )
    Invoke-FleetApi -FleetServer $FleetServer -Path "/api/hosts/$HostId" `
        -ApiKey $ApiKey -SkipCertificateCheck:$SkipCertificateCheck
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

# The estate manifest (.fleet-machines.json by default): one entry per machine
# provision-fleet-machines.ps1 created, read back by -Destroy, collect-logs.ps1
# and run-estate.ps1.
function Read-FleetManifest {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path $Path)) { return @() }
    # PS 5.1's ConvertFrom-Json emits a JSON array as ONE object; without the
    # ForEach-Object a caller's @(Read-FleetManifest) gets a one-element array
    # holding the real array, and $m.Name then yields every machine's name.
    try { return @(Get-Content -Path $Path -Raw | ConvertFrom-Json | ForEach-Object { $_ }) }
    catch {
        Write-Warning "Could not read the manifest $Path ($($_.Exception.Message)); treating it as empty."
        return @()
    }
}

function Write-FleetManifest {
    param([Parameter(Mandatory)][string]$Path, $Machines)
    # ConvertTo-Json unrolls a single-element array into an object, which would
    # then read back as one machine's worth of properties - force an array.
    $json = ConvertTo-Json -InputObject @($Machines) -Depth 5
    if (@($Machines).Count -eq 1 -and -not $json.StartsWith("[")) { $json = "[$json]" }
    Set-Content -Path $Path -Value $json -Force
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
        # Use this as the <role> part instead of a random one (an estate role
        # such as "mysql", so the machine says what it is for).
        [string]$Role = "",
        [int]$MaxAttempts = 200
    )
    $taken = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($e in $Exclude) { if ($e) { [void]$taken.Add($e) } }
    $roles = if ($Role) { @($Role) } else { $script:FleetNameRoles }

    for ($i = 0; $i -lt $MaxAttempts; $i++) {
        $name = "{0}-{1}-{2:d2}" -f (Get-Random -InputObject $roles),
        (Get-Random -InputObject $script:FleetNameSites),
        (Get-Random -Minimum 1 -Maximum 20)
        if (-not $taken.Contains($name)) { return $name }
    }
    # 20*20*19 combinations, so this only happens if a caller asks for an absurd
    # number of machines - fall back to a suffix rather than looping forever.
    return ("{0}-{1}-{2:d2}x{3}" -f (Get-Random -InputObject $roles),
        (Get-Random -InputObject $script:FleetNameSites),
        (Get-Random -Minimum 1 -Maximum 20), (Get-Random -Minimum 100 -Maximum 999))
}
