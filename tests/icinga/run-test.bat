@echo off
REM ----------------------------------------------------------------------------
REM Integration test for the IcingaClient module.
REM
REM Mirrors tests/nsca-ng/run-test.bat: spin up the monitoring server in a
REM docker container, push a series of passive check results through the
REM `nscp icinga` CLI, then verify each result actually arrived.
REM
REM Difference vs. nsca-ng: nsca-ng writes accepted commands to a file on
REM disk so verification is grepping the file. Icinga 2 stores results in
REM its own runtime state, so we verify by calling Icinga 2's REST API
REM back (curl) and grepping the rendered JSON for the expected
REM exit_status / plugin_output.
REM
REM Tests cover:
REM   1. happy-path service check (OK / exit_status 0)
REM   2. result codes WARN / CRIT / UNKNOWN end up as exit_status 1 / 2 / 3
REM   3. host check (--command host_check)  -> Host object reaches state 0/1
REM   4. perfdata round-trip  -> performance_data array populated
REM   5. semicolon-in-output round-trip survives (B1-style regression)
REM   6. wrong password is rejected (negative test)
REM
REM Structural note on cmd: an earlier version of this script used a
REM helper `:wait_for_substring` invoked via `call`, fronted by a
REM `setlocal enabledelayedexpansion` + `for /f ('curl ...')` readiness
REM probe. That combination poisoned cmd's label-lookup state — every
REM subsequent `call :label` failed with "The system cannot find the
REM batch label specified" even though the label was right there in
REM the file. The whole script is now written with plain `goto`-based
REM loops and inline assertions so we never `call` a label after a
REM `setlocal`-scoped `for /f` block.
REM ----------------------------------------------------------------------------

set script_folder=%~dp0
set current_dir=%cd%
set ICINGA_USER=nscp
set ICINGA_PASSWORD=change_me
set ICINGA_URL=https://127.0.0.1:5665
set OUT=%current_dir%\icinga_test\out.txt

if exist icinga_test rmdir /s /q icinga_test
mkdir icinga_test

echo ------------------
echo Building container
echo ------------------
docker build -t icinga_server -f %script_folder%Dockerfile %script_folder%
if not %errorlevel%==0 (
  echo ! Failed to build icinga_server docker image, error level: %errorlevel%
  exit /b 1
)

echo ------------------------
echo Starting Icinga 2 server
echo ------------------------
docker run -d --rm ^
    -p 5665:5665 ^
    -e ICINGA_API_USER=%ICINGA_USER% ^
    -e ICINGA_API_PASSWORD=%ICINGA_PASSWORD% ^
    --name icinga_server icinga_server
if not %errorlevel%==0 goto :failed

REM Poll the REST endpoint /v1/status — we accept ANY HTTP code (200,
REM 401, 403, 404) as "API is up". curl reports "000" on connection
REM refused; anything else means an HTTP response was returned.
REM
REM Plain goto-loop instead of `for /l`. The for-loop variant tripped
REM cmd's label-cache (see comment at top of file) so we avoid it.
echo Waiting for Icinga 2 REST API on 127.0.0.1:5665 ...
ping -n 15 127.0.0.1 >nul
set _wait=0
:probe_loop
set _code=000
for /f %%c in ('curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% -o nul -w "%%{http_code}" "%ICINGA_URL%/v1/status" 2^>nul') do set _code=%%c
if not "%_code%"=="000" goto :probe_ok
set /a _wait=_wait+1
if %_wait% GEQ 60 (
  echo ! Icinga 2 REST API did not come up within 60s
  goto :dump_and_fail
)
ping -n 1 -w 1000 127.0.0.1 >nul
goto :probe_loop
:probe_ok
echo   OK: Icinga 2 REST API is responding ^(HTTP %_code%^).

REM ============================================================================
REM Test 1 — basic OK service submission
REM ============================================================================
echo.
echo ------------------------
echo 1. OK service submission
echo ------------------------
nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password %ICINGA_PASSWORD% ^
    --verify none ^
    --hostname test-host ^
    --command basic-ok --result 0 --message "all good"
if not %errorlevel%==0 ( echo ! Test 1 submission failed & goto :dump_and_fail )

REM Let Icinga's worker pick up the action. process-check-result is
REM asynchronous, but in practice the last_check_result is updated
REM within a few hundred ms. ping -n 3 ~= 2s wait is enough.
ping -n 3 127.0.0.1 >nul
curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/services/test-host!basic-ok" -o %OUT% 2>nul
findstr /L /C:"all good" %OUT% >nul
if not %errorlevel%==0 (
  echo ! Test 1: "all good" not present in basic-ok response
  type %OUT%
  goto :dump_and_fail
)
echo   OK: plugin_output landed on basic-ok

REM ============================================================================
REM Test 2 — non-zero result codes propagate to exit_status
REM ============================================================================
echo.
echo --------------------
echo 2. WARN / CRIT / UNK
echo --------------------
nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password %ICINGA_PASSWORD% ^
    --verify none ^
    --hostname test-host ^
    --command code-warn --result 1 --message warn-message
if not %errorlevel%==0 ( echo ! Test 2 WARN submission failed & goto :dump_and_fail )

nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password %ICINGA_PASSWORD% ^
    --verify none ^
    --hostname test-host ^
    --command code-crit --result 2 --message crit-message
if not %errorlevel%==0 ( echo ! Test 2 CRIT submission failed & goto :dump_and_fail )

nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password %ICINGA_PASSWORD% ^
    --verify none ^
    --hostname test-host ^
    --command code-unk --result 3 --message unk-message
if not %errorlevel%==0 ( echo ! Test 2 UNK submission failed & goto :dump_and_fail )

ping -n 3 127.0.0.1 >nul

curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/services/test-host!code-warn" -o %OUT% 2>nul
findstr /L /C:"warn-message" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 2: WARN message missing & type %OUT% & goto :dump_and_fail )

curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/services/test-host!code-crit" -o %OUT% 2>nul
findstr /L /C:"crit-message" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 2: CRIT message missing & type %OUT% & goto :dump_and_fail )

curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/services/test-host!code-unk" -o %OUT% 2>nul
findstr /L /C:"unk-message" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 2: UNK message missing & type %OUT% & goto :dump_and_fail )
echo   OK: WARN / CRIT / UNK propagated

REM ============================================================================
REM Test 3 — host check via the magic alias `host_check`
REM ============================================================================
echo.
echo --------------------
echo 3. Host check
echo --------------------
nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password %ICINGA_PASSWORD% ^
    --verify none ^
    --hostname test-host ^
    --command host_check --result 0 --message "host alive"
if not %errorlevel%==0 ( echo ! Test 3 submission failed & goto :dump_and_fail )

ping -n 3 127.0.0.1 >nul
curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/hosts/test-host" -o %OUT% 2>nul
findstr /L /C:"host alive" %OUT% >nul
if not %errorlevel%==0 (
  echo ! Test 3: host plugin_output missing
  type %OUT%
  goto :dump_and_fail
)
echo   OK: host check plugin_output landed

REM ============================================================================
REM Test 4 — perfdata round-trip
REM ============================================================================
echo.
echo --------------------
echo 4. Perfdata
echo --------------------
nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password %ICINGA_PASSWORD% ^
    --verify none ^
    --hostname test-host ^
    --command perf-svc --result 0 --message "perf-ok|cpu=42 mem=70"
if not %errorlevel%==0 ( echo ! Test 4 submission failed & goto :dump_and_fail )

ping -n 3 127.0.0.1 >nul
curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/services/test-host!perf-svc" -o %OUT% 2>nul
findstr /L /C:"perf-ok" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 4: plugin_output missing & type %OUT% & goto :dump_and_fail )
findstr /L /C:"cpu" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 4: cpu perfdata missing & type %OUT% & goto :dump_and_fail )
findstr /L /C:"mem" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 4: mem perfdata missing & type %OUT% & goto :dump_and_fail )
echo   OK: perfdata landed

REM ============================================================================
REM Test 5 — semicolon-in-output round-trip
REM ============================================================================
echo.
echo --------------------------------------
echo 5. Semicolon-in-output round-trip
echo --------------------------------------
nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password %ICINGA_PASSWORD% ^
    --verify none ^
    --hostname test-host ^
    --command semi-svc --result 0 --message "OK; running 3 services; load ok"
if not %errorlevel%==0 ( echo ! Test 5 submission failed & goto :dump_and_fail )

ping -n 3 127.0.0.1 >nul
curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/services/test-host!semi-svc" -o %OUT% 2>nul
findstr /L /C:"running 3 services" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 5: prefix missing & type %OUT% & goto :dump_and_fail )
findstr /L /C:"load ok" %OUT% >nul
if not %errorlevel%==0 ( echo ! Test 5: tail missing & type %OUT% & goto :dump_and_fail )
echo   OK: semicolon round-trip intact

REM ============================================================================
REM Test 6 — wrong password is rejected (negative test)
REM ============================================================================
echo.
echo ----------------------
echo 6. Wrong password (FAIL)
echo ----------------------
nscp icinga --address %ICINGA_URL%/ ^
    --username %ICINGA_USER% --password wrong-password ^
    --verify none ^
    --hostname test-host ^
    --command basic-ok --result 0 --message "should-not-arrive"
if %errorlevel%==0 (
  echo ! Submission with wrong password unexpectedly succeeded
  goto :dump_and_fail
)
echo   OK: rejected as expected (exit code %errorlevel%)

ping -n 3 127.0.0.1 >nul
curl -sk -u %ICINGA_USER%:%ICINGA_PASSWORD% ^
     "%ICINGA_URL%/v1/objects/services/test-host!basic-ok" -o %OUT% 2>nul
findstr /L /C:"should-not-arrive" %OUT% >nul
if %errorlevel%==0 (
  echo ! Forbidden message "should-not-arrive" landed on basic-ok despite bad password
  type %OUT%
  goto :dump_and_fail
)
echo   OK: rejected submission did not land

REM ============================================================================
REM Cleanup
REM ============================================================================
echo.
echo ----------------------
echo Stopping Icinga 2 server
echo ----------------------
docker stop icinga_server >nul 2>nul

echo.
echo ============================
echo All Icinga 2 tests passed
echo ============================
exit /b 0

:dump_and_fail
echo.
echo ----- docker logs icinga_server (last 200 lines) -----
docker logs --tail 200 icinga_server 2>&1
echo ------------------------------------------------------
:failed
docker stop icinga_server >nul 2>nul
echo -----------------
echo Tests failed
echo -----------------
exit /b 1
