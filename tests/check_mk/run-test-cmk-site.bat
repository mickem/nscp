@echo off
REM ---------------------------------------------------------------------------
REM End-to-end test against a real Checkmk site.
REM
REM Pulls the checkmk/check-mk-raw image (~500MB), starts an OMD site, registers
REM our nscp instance as a Checkmk agent host via the REST API, and runs
REM `cmk -d / -II / -v` to validate that the agent dump is fully consumable.
REM
REM Manual / pre-release only. Slower than run-test.bat by ~1-2 minutes due to
REM the site bootstrap, but exercises the actual Checkmk parser and discovery
REM logic instead of just substring assertions.
REM
REM Override IMAGE via env to pin a specific Checkmk version, e.g.:
REM   set IMAGE=checkmk/check-mk-raw:2.3.0-latest
REM ---------------------------------------------------------------------------

REM No `setlocal enabledelayedexpansion` here on purpose: it makes `!` a
REM variable-expansion delimiter and silently eats the leading `!` in error
REM messages. We don't use !var! anywhere in this script.
set script_folder=%~dp0
set TESTRESULT=1
set SITE=cmk
if "%IMAGE%"==""        set IMAGE=checkmk/check-mk-raw:latest
set HOST=nscp-test
set CONTAINER=cmk-e2e-site
set CMK_USER=cmkadmin
set CMK_PASSWORD=cmk_e2e_password
set CMK_PORT=5000
set BASE_URL=http://localhost:%CMK_PORT%/%SITE%/check_mk/api/1.0
set OUT=mk_e2e

if exist %OUT% rmdir /s /q %OUT%
mkdir %OUT%

echo ------------------------------
echo Configuring nscp test instance
echo ------------------------------
nscp settings --path "/modules" --key CheckMKServer --set "enabled"
nscp settings --path "/modules" --key LUAScript --set "enabled"
nscp settings --path "/modules" --key CheckSystem --set "enabled"
nscp settings --path "/modules" --key CheckHelpers --set "enabled"
nscp settings --path "/modules" --key CheckDisk --set "enabled"
nscp settings --path "/modules" --key Scheduler --set "enabled"
nscp settings --path "/settings/check_mk/server" --key port --set 6556
REM Allow the docker bridge networks. Adjust if your local docker uses a
REM different subnet (run `docker network inspect bridge` to check).
nscp settings --path "/settings/check_mk/server" --key "allowed hosts" --set "127.0.0.1,::1,172.16.0.0/12,192.168.0.0/16,10.0.0.0/8"
nscp settings --path "/settings/check_mk/server/local" --key "CPU Load" --set "command=check_cpu"
nscp settings --path "/settings/check_mk/server/mrpe" --key Uptime    --set "command=check_uptime"
REM Scheduler-driven passive checks landing on the new submission channels.
nscp settings --path "/settings/scheduler/schedules/default" --key channel  --set "check_mk-mrpe"
nscp settings --path "/settings/scheduler/schedules/default" --key interval --set "5s"
nscp settings --path "/settings/scheduler/schedules/default" --key report   --set "all"
nscp settings --path "/settings/scheduler/schedules/Scheduled_OK"      --key command --set "check_ok"
nscp settings --path "/settings/scheduler/schedules/Scheduled_Warning" --key command --set "check_always_warning check_ok"
nscp settings --path "/settings/scheduler/schedules/Scheduled_Warning" --key channel --set "check_mk-local"
nscp nrpe install --allowed-hosts 127.0.0.1 --insecure --verify=none
nscp lua install
nscp lua add --script mock

echo - Starting nscp test instance...
start "nscp-cmk-e2e" nscp test
timeout /t 4 /nobreak >nul

echo ---------------------
echo Starting Checkmk site
echo ---------------------
docker rm -f %CONTAINER% >nul 2>&1
docker pull %IMAGE%
if not %errorlevel%==0 (
  echo ! Failed to pull %IMAGE%
  goto :shutdown
)
docker run -d --name %CONTAINER% -p %CMK_PORT%:5000 ^
  -e CMK_SITE_ID=%SITE% -e CMK_PASSWORD=%CMK_PASSWORD% ^
  --tmpfs /omd/sites/%SITE%/tmp:rw,exec ^
  %IMAGE%
if not %errorlevel%==0 (
  echo ! Failed to start container
  goto :shutdown
)

echo - Waiting for site to come up (up to 240s)...
set /a RETRIES=120
:wait_site
curl -s -f -u %CMK_USER%:%CMK_PASSWORD% %BASE_URL%/version >nul 2>&1
if %errorlevel%==0 goto :site_up
set /a RETRIES-=1
if %RETRIES% leq 0 (
  echo ! Checkmk site failed to start
  docker logs --tail 80 %CONTAINER%
  goto :shutdown
)
timeout /t 2 /nobreak >nul
goto :wait_site

:site_up
echo - Site is up.

echo ----------------
echo Registering host
echo ----------------
REM Use -f so curl returns non-zero on any HTTP error - far simpler than
REM capturing %%{http_code} via `set /p` which doesn't read files lacking a
REM trailing newline. Body still goes to %OUT%\create.json for diagnostics.
curl -s -f -X POST ^
  -u %CMK_USER%:%CMK_PASSWORD% ^
  -H "Accept: application/json" ^
  -H "Content-Type: application/json" ^
  %BASE_URL%/domain-types/host_config/collections/all ^
  -d "{\"folder\":\"/\",\"host_name\":\"%HOST%\",\"attributes\":{\"ipaddress\":\"host.docker.internal\",\"tag_agent\":\"cmk-agent\",\"tag_address_family\":\"ip-v4-only\"}}" ^
  -o %OUT%\create.json
if errorlevel 1 (
  echo ! Host create failed - see %OUT%\create.json
  type %OUT%\create.json
  goto :shutdown
)
echo - Host registered.

echo - Activating changes...
curl -s -f -X POST ^
  -u %CMK_USER%:%CMK_PASSWORD% ^
  -H "Accept: application/json" ^
  -H "Content-Type: application/json" ^
  -H "If-Match: *" ^
  %BASE_URL%/domain-types/activation_run/actions/activate-changes/invoke ^
  -d "{\"redirect\":false,\"sites\":[\"%SITE%\"],\"force_foreign_changes\":false}" ^
  -o %OUT%\activate.json
if errorlevel 1 (
  echo ! Activate changes failed - see %OUT%\activate.json
  type %OUT%\activate.json
  goto :shutdown
)
timeout /t 3 /nobreak >nul

REM `cmk` is at /omd/sites/<site>/bin/cmk; that directory is in the site user's
REM PATH only via the login profile, which `docker exec -u <site>` does not
REM source. Invoke through `bash -lc` so .bash_profile sets PATH (and pylib,
REM CMK_BASE_DIR, etc.) before cmk runs.
set CMK=docker exec -u %SITE% %CONTAINER% bash -lc

echo --------------------
echo cmk -d (raw dump)
echo --------------------
%CMK% "cmk -d %HOST%" > %OUT%\agent.txt 2>&1
echo --- begin agent dump ---
type %OUT%\agent.txt
echo --- end agent dump ---

echo --------------------
echo cmk -II (discovery)
echo --------------------
%CMK% "cmk -II %HOST%" > %OUT%\discovery.txt 2>&1
type %OUT%\discovery.txt

echo --------------------
echo cmk -v (run checks)
echo --------------------
%CMK% "cmk -v %HOST%" > %OUT%\check.txt 2>&1
type %OUT%\check.txt

echo --------------------
echo Assertions
echo --------------------
REM Use explicit `if errorlevel 1 goto :shutdown` instead of `||` because
REM errorlevel propagation through `call :label || ...` is unreliable when
REM the called function uses parens-in-content. Match body lines (not
REM `^<^<^<headers^>^>^>` - CMD `||` re-parse mangles `^<^<` even inside quotes).
call :assert "Version: nsclient++"   "check_mk header in raw dump"      %OUT%\agent.txt
if errorlevel 1 goto :shutdown
call :assert "MemTotal:"             "mem MemTotal in raw dump"         %OUT%\agent.txt
if errorlevel 1 goto :shutdown
call :assert "CPU Load"              "local CPU Load in raw dump"       %OUT%\agent.txt
if errorlevel 1 goto :shutdown

REM Phase 2 - scheduled passive results submitted via check_mk-mrpe / -local.
call :assert "Scheduled_OK"          "scheduled MRPE entry in raw dump"  %OUT%\agent.txt
if errorlevel 1 goto :shutdown
call :assert "Scheduled_Warning"     "scheduled local entry in raw dump" %OUT%\agent.txt
if errorlevel 1 goto :shutdown
REM Label intentionally paren-free - any `^)` would close the `if errorlevel ^(...^)`
REM block inside the assert function and CMD would error with "was unexpected".
call :assert "cached("               "cached prefix in raw dump"         %OUT%\agent.txt
if errorlevel 1 goto :shutdown

REM Service display names show up in `cmk -v` output, not in `cmk -II` output
REM (which lists plugin technical names like `mem` / `uptime` / `local`).
call :assert "Filesystem"            "Filesystem service in cmk -v"     %OUT%\check.txt
if errorlevel 1 goto :shutdown
call :assert "Uptime"                "Uptime service in cmk -v"         %OUT%\check.txt
if errorlevel 1 goto :shutdown
call :assert "Service Summary"       "Service Summary in cmk -v"        %OUT%\check.txt
if errorlevel 1 goto :shutdown
call :assert "CPU Load"              "CPU Load local-check in cmk -v"   %OUT%\check.txt
if errorlevel 1 goto :shutdown

REM Scheduled passive entries should be discovered as services too. MRPE
REM names show up under `MRPE Scheduled_OK`; local entries use the name
REM directly as the Checkmk service display name.
call :assert "Scheduled_OK"          "scheduled MRPE service in cmk -v"   %OUT%\check.txt
if errorlevel 1 goto :shutdown
call :assert "Scheduled_Warning"     "scheduled local service in cmk -v"  %OUT%\check.txt
if errorlevel 1 goto :shutdown

REM `cmk -v` should not emit `Invalid data:` (parser rejected a section) or
REM `UNKNOWN - agent` (could not contact agent).
findstr /c:"Invalid data:" %OUT%\check.txt >nul
if not errorlevel 1 (
  echo ! cmk -v reported Invalid data - parser rejected a section
  goto :shutdown
)
findstr /c:"UNKNOWN - agent" %OUT%\check.txt >nul
if not errorlevel 1 (
  echo ! cmk -v could not contact agent
  goto :shutdown
)
echo - OK: cmk -v ran without Invalid data / UNKNOWN agent errors

echo --------------------------------
echo All end-to-end tests passed
echo --------------------------------
set TESTRESULT=0
goto :shutdown


:shutdown
echo - Tearing down checkmk container...
docker rm -f %CONTAINER% >nul 2>&1
echo - Stopping nscp test instance...
nscp nrpe --host 127.0.0.1 --insecure --version 2 --command mock_exit >nul 2>&1
timeout /t 2 /nobreak >nul
taskkill /f /fi "WINDOWTITLE eq nscp-cmk-e2e*" >nul 2>&1
taskkill /f /im nscp.exe >nul 2>&1
exit /b %TESTRESULT%


:: ---------------------------------------------------------------------------
:: assert SUBSTRING LABEL FILE
:: ---------------------------------------------------------------------------
:assert
findstr /c:%1 %3 >nul
if errorlevel 1 (
  echo ! Missing %~2 ^(expected %~1 in %~3^)
  exit /b 1
)
echo - OK: %~2
exit /b 0
