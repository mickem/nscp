# SQL Server for the mssql estate role. The VM is the Marketplace SQL Server
# 2022 Developer image, so the engine is already installed; this makes it
# reachable the way CheckMSSQL connects. Runs as SYSTEM before NSClient++ is
# installed, with ROLE_PASSWORD in the environment (see role.json and
# README.md, "Adding a role"). The agent connects as the SQL login nscp over
# TCP to 127.0.0.1 with ODBC Driver 18; the role bundle carries the same
# password into [/settings/mssql].
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
if (-not $env:ROLE_PASSWORD) { throw "ROLE_PASSWORD is not set" }
$password = $env:ROLE_PASSWORD

# The x64 installer of ODBC Driver 18.7, which no longer needs the Visual C++
# redistributable. The fwlink moves with every release; a 404 here fails the
# install stage with the url in the log.
$odbcUrl = 'https://go.microsoft.com/fwlink/?linkid=2378279'
$odbcDriver = 'ODBC Driver 18 for SQL Server'
# Only this connection may log in while the engine is in single-user mode, so
# the SQL IaaS extension cannot take the one slot first.
$setupApp = 'NSCPROLESETUP'

function Write-Step([string]$Text) { Write-Output "== $Text" }

# --- the instance --------------------------------------------------------------

$names = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Microsoft SQL Server\Instance Names\SQL'
$instance = @($names.PSObject.Properties | Where-Object { $_.Name -notlike 'PS*' })[0]
if (-not $instance) { throw "No SQL Server instance is registered on this image." }
$instanceName = $instance.Name
$instanceId = "$($instance.Value)"
if ($instanceName -eq 'MSSQLSERVER') {
    $engine = 'MSSQLSERVER'; $agent = 'SQLSERVERAGENT'
}
else {
    $engine = "MSSQL`$$instanceName"; $agent = "SQLAgent`$$instanceName"
}
Write-Step "instance $instanceName ($instanceId), services $engine / $agent"
$instanceKey = "HKLM:\SOFTWARE\Microsoft\Microsoft SQL Server\$instanceId\MSSQLServer"

# --- TCP on 1433 and mixed-mode authentication ----------------------------------

# Both are read at engine start, so they go in before the restarts below.
$tcp = "$instanceKey\SuperSocketNetLib\Tcp"
Set-ItemProperty -Path $tcp -Name Enabled -Value 1
Set-ItemProperty -Path "$tcp\IPAll" -Name TcpPort -Value '1433'
Set-ItemProperty -Path "$tcp\IPAll" -Name TcpDynamicPorts -Value ''
Set-ItemProperty -Path $instanceKey -Name LoginMode -Value 2
Write-Step "TCP 1433 enabled, mixed-mode authentication on"

function Wait-Engine {
    $svc = Get-Service -Name $engine
    $svc.WaitForStatus('Running', [TimeSpan]::FromMinutes(3))
}

function Stop-Engine {
    # -Force takes the Agent (a dependent) down with it.
    Stop-Service -Name $engine -Force
    (Get-Service -Name $engine).WaitForStatus('Stopped', [TimeSpan]::FromMinutes(3))
}

# Run a batch, retrying the connect while the engine finishes recovery.
function Invoke-Sql {
    param([string]$ConnectionString, [string]$Sql, [int]$Attempts = 30)
    for ($i = 1; ; $i++) {
        $conn = New-Object System.Data.SqlClient.SqlConnection $ConnectionString
        try {
            $conn.Open()
            $cmd = $conn.CreateCommand()
            $cmd.CommandText = $Sql
            return $cmd.ExecuteScalar()
        }
        catch [System.Data.SqlClient.SqlException] {
            if ($i -ge $Attempts) { throw }
            Write-Output "   connect attempt $i failed ($($_.Exception.Message.Split("`n")[0])); retrying"
            Start-Sleep -Seconds 5
        }
        finally { $conn.Dispose() }
    }
}

# --- the nscp login ----------------------------------------------------------

# SYSTEM is not a sysadmin on this image, but in single-user mode every member
# of the local Administrators group is, and SYSTEM is one. That is the one way
# in that does not depend on how the image provisioned its logins.
Write-Step "restarting $engine in single-user mode"
Stop-Engine
(Get-Service -Name $engine).Start(@("-m$setupApp"))
Wait-Engine

# CHECK_POLICY=OFF: the password is a throwaway (letters and digits) that
# need not meet the Windows complexity rules. ALTER as well as CREATE so a
# re-run resets it. sysadmin because the checks read msdb, the DMVs and
# every database; this is a throwaway server on a throwaway estate.
$escaped = $password.Replace("'", "''")
$sql = @"
IF NOT EXISTS (SELECT 1 FROM sys.server_principals WHERE name = N'nscp')
    CREATE LOGIN [nscp] WITH PASSWORD = N'$escaped', CHECK_POLICY = OFF, CHECK_EXPIRATION = OFF;
ALTER LOGIN [nscp] WITH PASSWORD = N'$escaped', CHECK_POLICY = OFF, CHECK_EXPIRATION = OFF;
ALTER LOGIN [nscp] ENABLE;
IF IS_SRVROLEMEMBER(N'sysadmin', N'nscp') = 0 ALTER SERVER ROLE [sysadmin] ADD MEMBER [nscp];
SELECT 1;
"@
$local = "Server=.;Integrated Security=SSPI;Application Name=$setupApp;Connect Timeout=10"
Invoke-Sql -ConnectionString $local -Sql $sql | Out-Null
Write-Step "login nscp created"

Write-Step "restarting $engine in normal mode"
Stop-Engine
Start-Service -Name $engine
Wait-Engine
# The Agent owns the jobs check_mssql_jobs reads; keep it running like a
# production server's.
Set-Service -Name $agent -StartupType Automatic
Start-Service -Name $agent
Write-Step "$agent running"

# --- ODBC Driver 18 ------------------------------------------------------------

if (Get-OdbcDriver -Name $odbcDriver -Platform '64-bit' -ErrorAction SilentlyContinue) {
    Write-Step "$odbcDriver already installed"
}
else {
    Write-Step "installing $odbcDriver from $odbcUrl"
    $msi = 'C:\temp\msodbcsql18.msi'
    Invoke-WebRequest -Uri $odbcUrl -OutFile $msi -UseBasicParsing
    $p = Start-Process msiexec.exe -Wait -PassThru -ArgumentList @(
        '/i', $msi, '/qn', 'IACCEPTMSODBCSQLLICENSETERMS=YES', '/l*v', 'C:\temp\msodbcsql18.log')
    if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) {
        Get-Content 'C:\temp\msodbcsql18.log' -Tail 40 -ErrorAction SilentlyContinue
        throw "msodbcsql18 install failed with exit code $($p.ExitCode)"
    }
    if (-not (Get-OdbcDriver -Name $odbcDriver -Platform '64-bit' -ErrorAction SilentlyContinue)) {
        throw "msiexec succeeded but '$odbcDriver' is not registered"
    }
}

# --- prove the login the agent will make --------------------------------------

# The same driver, transport and credentials as the agent, so a broken account
# or a closed port fails the install stage here rather than surfacing later as
# a failed check. TrustServerCertificate as the role's trust-cert=true: Driver
# 18 encrypts by default and the instance certificate is self-signed.
$odbc = "Driver={$odbcDriver};Server=127.0.0.1,1433;UID=nscp;PWD=$password;TrustServerCertificate=yes;APP=nscp-role-install"
for ($i = 1; ; $i++) {
    $conn = New-Object System.Data.Odbc.OdbcConnection $odbc
    try {
        $conn.Open()
        $cmd = $conn.CreateCommand()
        $cmd.CommandText = "SELECT CAST(SERVERPROPERTY('ProductVersion') AS nvarchar(32)) + ' ' + CAST(SERVERPROPERTY('Edition') AS nvarchar(64))"
        Write-Step "connected over ODBC as nscp: SQL Server $($cmd.ExecuteScalar())"
        break
    }
    catch {
        if ($i -ge 30) { throw }
        Write-Output "   ODBC attempt $i failed ($($_.Exception.Message.Split("`n")[0])); retrying"
        Start-Sleep -Seconds 5
    }
    finally { $conn.Dispose() }
}
exit 0
