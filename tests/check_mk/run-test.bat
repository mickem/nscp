@echo off
set script_folder=%~dp0
set TESTRESULT=1

if exist mk_test rmdir /s /q mk_test
mkdir mk_test

echo ------------------
echo Building container
echo ------------------
docker build -t check_mk_client -f %script_folder%Dockerfile %script_folder%
if not %errorlevel%==0 (
  echo ! Failed to build check_mk_client docker image, Error level was: %errorlevel%
  exit /b 1
)

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
REM Allow loopback plus the common Docker bridge ranges. Run
REM `docker network inspect bridge` if your daemon uses something exotic.
nscp settings --path "/settings/check_mk/server" --key "allowed hosts" --set "127.0.0.1,::1,172.16.0.0/12,192.168.0.0/16,10.0.0.0/8"

echo - Configuring local-check / mrpe entries...
nscp settings --path "/settings/check_mk/server/local" --key "CPU Load" --set "command=check_cpu"
nscp settings --path "/settings/check_mk/server/mrpe" --key Uptime    --set "command=check_uptime"

echo - Configuring scheduler-driven passive checks (Phase 2)...
REM Default for the schedules group: 5s interval, all-status report, MRPE channel.
nscp settings --path "/settings/scheduler/schedules/default" --key channel  --set "check_mk-mrpe"
nscp settings --path "/settings/scheduler/schedules/default" --key interval --set "5s"
nscp settings --path "/settings/scheduler/schedules/default" --key report   --set "all"
REM One schedule inheriting the default (lands on check_mk-mrpe).
nscp settings --path "/settings/scheduler/schedules/Scheduled_OK"      --key command --set "check_ok"
REM A second schedule overriding the channel to land on check_mk-local instead.
REM check_always_warning is a wrapper that requires a sub-command - chain it
REM through check_ok so the wrapped run produces output, just forced to WARN.
nscp settings --path "/settings/scheduler/schedules/Scheduled_Warning" --key command --set "check_always_warning check_ok"
nscp settings --path "/settings/scheduler/schedules/Scheduled_Warning" --key channel --set "check_mk-local"

echo - Installing NRPE (used as the shutdown channel) and lua mock...
nscp nrpe install --allowed-hosts 127.0.0.1 --insecure --verify=none
nscp lua install
nscp lua add --script mock

echo - Starting nscp test instance...
start "nscp-check_mk-test" nscp test

timeout /t 10 /nobreak >nul

echo --------------------
echo Fetching agent dump
echo --------------------
docker run --rm check_mk_client host.docker.internal 6556 > mk_test\agent.txt 2>&1
if not %errorlevel%==0 (
  echo ! Failed to fetch agent dump, Error level was: %errorlevel%
  goto :shutdown
)
echo --- begin agent dump ---
type mk_test\agent.txt
echo --- end agent dump ---

REM Sanity: empty dump means the connection was rejected (allowed-hosts) or
REM CheckMKServer didn't bind. Bail with a clear message before assertions.
for %%A in (mk_test\agent.txt) do if %%~zA==0 (
  echo ! Agent dump is empty. Likely allowed-hosts blocked the docker bridge IP
  echo   or CheckMKServer is not listening on 6556. Inspect the test instance window.
  goto :shutdown
)

REM Assertions target body content rather than `^<^<^<section^>^>^>` headers
REM because CMD's parser mangles `^<^<` even inside double quotes when `||` is
REM on the same line. Body lines are unique enough to verify each section.
call :assert "Version: nsclient++"   "check_mk Version line"      || goto :shutdown
call :assert "AgentOS:"              "check_mk AgentOS line"      || goto :shutdown
call :assert "Hostname:"             "check_mk Hostname line"    || goto :shutdown
call :assert "MemTotal:"             "mem section MemTotal"       || goto :shutdown
call :assert "CPU Load"              "local CPU Load entry"       || goto :shutdown
REM Match any return code; check_uptime emits WARN when uptime is short. Avoid
REM parens in the label - they close the surrounding `if errorlevel (...)`.
call :assert "Uptime "               "mrpe Uptime entry"          || goto :shutdown

REM Phase 2: passive submissions delivered by the Scheduler module at the
REM 5-second interval configured above. By the time we fetch the agent dump
REM (10s after start) each schedule has fired at least once, the result is
REM stored in CheckMKServer's submission cache, and emitted with a
REM `cached(epoch, ttl)` prefix.
call :assert "Scheduled_OK"          "scheduled MRPE entry"       || goto :shutdown
call :assert "Scheduled_Warning"     "scheduled local entry"      || goto :shutdown
REM Label intentionally paren-free - any `^)` would close the `if errorlevel ^(...^)`
REM block inside the assert function and CMD would error with "was unexpected".
call :assert "cached("               "cached prefix"              || goto :shutdown

echo --------------------------------
echo All tests completed successfully
echo --------------------------------
set TESTRESULT=0
goto :shutdown


:shutdown
echo - Shutting down test instance via NRPE mock_exit...
nscp nrpe --host 127.0.0.1 --insecure --version 2 --command mock_exit >nul 2>&1
timeout /t 2 /nobreak >nul
REM No `taskkill /im nscp.exe` here on purpose: it would nuke every nscp.exe
REM on the box, including any installed service or unrelated CLI invocation,
REM and on some setups that ends up tearing down the calling shell. mock_exit
REM is enough; if it ever hangs, kill the leftover window manually.
exit /b %TESTRESULT%


:: ---------------------------------------------------------------------------
:: assert SUBSTRING LABEL
::
:: Looks for SUBSTRING in mk_test\agent.txt. On miss, prints the label and
:: returns errorlevel 1 so the caller can `|| goto :shutdown`.
:: ---------------------------------------------------------------------------
:assert
findstr /c:%1 mk_test\agent.txt >nul
if errorlevel 1 (
  echo ! Missing %~2 ^(expected %~1^)
  exit /b 1
)
echo - OK: %~2
exit /b 0
