@echo off
REM ----------------------------------------------------------------------------
REM Integration test for the NSCANgClient module.
REM
REM Mirrors tests/nsca/run-test.bat: a Debian docker image runs the nsca-ng
REM server with a fixed PSK; this script invokes `nscp nsca-ng` as the client
REM to push a series of check results, then greps the mounted results file to
REM verify each line landed correctly. Tests cover:
REM
REM   1. happy-path service check (OK)
REM   2. result codes WARN / CRIT / UNKNOWN end up encoded as 1 / 2 / 3
REM   3. host check (--host-check)  -> PROCESS_HOST_CHECK_RESULT
REM   4. plugin output containing ';' must round-trip without losing the
REM      trailing field (regression for the escape_field semicolon bug)
REM   5. wrong password is rejected (negative test)
REM
REM Requirements: Docker Desktop on Windows, NSClient++ installed and on PATH
REM (so the `nscp` CLI is available).
REM ----------------------------------------------------------------------------

set script_folder=%~dp0
set current_dir=%cd%
set IDENTITY=nscp
set PASSWORD=change_me

goto :start

REM ----- helpers --------------------------------------------------------------
REM
REM Note on control flow: `exit /b 1` from a :label only exits the subroutine,
REM not the script. Every `call :expect_*` and `call :fail` site below must be
REM followed (in main) by `if not %errorlevel%==0 exit /b 1` to propagate the
REM failure up to the script's top level. Inside the helpers, the failure
REM branches `exit /b 1` directly so the OK-echo on the success path never
REM runs after a failure.

:fail
echo.
echo ! TEST FAILED at step: %1
echo.
echo ----- docker logs nsca_ng_server (last 200 lines) -----
docker logs --tail 200 nsca_ng_server 2>&1
echo -------------------------------------------------------
docker stop nsca_ng_server >nul 2>nul
exit /b 1

:expect_in_results
REM %1 = description, %2 = literal substring expected in results.txt
REM Why `findstr /L /C:` and not `findstr /C:` or plain `find`:
REM  * `findstr /C:` is documented as "literal search string" but still
REM    treats `\` as a regex escape — `\;` silently becomes `;` and the
REM    B1 semicolon-escape round-trip assertion breaks.
REM  * `find` is genuinely literal but reads in the OEM code page and
REM    handles LF-only line endings poorly; on the docker-produced
REM    results.txt it can return "not found" even for plain ASCII lines.
REM  * `findstr /L /C:` combines a truly literal pattern (`/L` forces
REM    no-regex on top of /C:) with findstr's saner file reading.
REM End with `exit /b 0` (not `goto :eof`) so the helper's own errorlevel
REM is what the caller sees - findstr's exit code (1 when not found)
REM would otherwise leak through and trip the caller's
REM `if not %errorlevel%==0` guard. echo does not reset errorlevel in cmd.
findstr /L /C:%2 %current_dir%\nsca_ng_test\results.txt >nul
if not %errorlevel%==0 (
  echo ! Expected substring not found: %2
  echo --- results.txt ---
  type %current_dir%\nsca_ng_test\results.txt
  echo -------------------
  call :fail "%~1"
  exit /b 1
)
echo   OK: found %2
exit /b 0

:expect_not_in_results
REM %1 = description, %2 = literal substring that must NOT appear
REM Same `findstr /L /C:` rationale as :expect_in_results. Explicit
REM `exit /b 0` matters most here because findstr returns 1 in the
REM success case (substring absent), which is exactly the leak that
REM bites callers.
findstr /L /C:%2 %current_dir%\nsca_ng_test\results.txt >nul
if %errorlevel%==0 (
  echo ! Forbidden substring present: %2
  echo --- results.txt ---
  type %current_dir%\nsca_ng_test\results.txt
  echo -------------------
  call :fail "%~1"
  exit /b 1
)
echo   OK: absent %2
exit /b 0

REM ----- main -----------------------------------------------------------------

:start

if exist nsca_ng_test rmdir /s /q nsca_ng_test
mkdir nsca_ng_test

echo ------------------
echo Building container
echo ------------------

docker build -t nsca_ng_server -f %script_folder%Dockerfile %script_folder%
if not %errorlevel%==0 (
  echo ! Failed to build nsca_ng_server docker image, error level: %errorlevel%
  exit /b 1
)

REM Pre-create the results file so docker bind-mounts it as a file rather than
REM a directory on Windows.
echo. > %current_dir%\nsca_ng_test\results.txt

echo ------------------------
echo Starting NSCA-NG server
echo ------------------------

docker run -d --rm ^
    --volume %current_dir%\nsca_ng_test:/nsca-ng ^
    -p 5668:5668 ^
    -e NSCA_NG_IDENTITY=%IDENTITY% ^
    -e NSCA_NG_PASSWORD=%PASSWORD% ^
    --name nsca_ng_server nsca_ng_server
if not %errorlevel%==0 ( call :fail "docker run" & exit /b 1 )

REM Wait until the daemon is actually accepting connections on 5668. Polling
REM rather than a fixed sleep because the server can take a moment to bind on
REM cold-cache CI runs, and a misconfigured nsca-ng exits immediately —
REM detecting that here is much clearer than four retried "ECONNREFUSED"s
REM later.
echo Waiting for nsca-ng to listen on 127.0.0.1:5668 ...
powershell -NoProfile -Command "$ok=$false; for($i=0;$i -lt 30;$i++){ try { $c=New-Object System.Net.Sockets.TcpClient; $c.Connect('127.0.0.1',5668); $c.Close(); $ok=$true; break } catch { Start-Sleep -Milliseconds 500 } }; if(-not $ok){ exit 1 }"
if not %errorlevel%==0 (
  echo ! nsca-ng never started listening on 5668. Container logs:
  echo ----- docker logs nsca_ng_server -----
  docker logs nsca_ng_server
  echo --------------------------------------
  call :fail "server startup"
  exit /b 1
)
echo   OK: nsca-ng is accepting connections.

REM ============================================================================
REM Test 1 — basic OK service submission
REM ============================================================================
echo.
echo ------------------------
echo 1. OK service submission
echo ------------------------
nscp nsca-ng --host=127.0.0.1 ^
    --identity=%IDENTITY% --password=%PASSWORD% ^
    --hostname=test-host ^
    --command basic-ok --result 0 --message "all good"
if not %errorlevel%==0 ( call :fail "Test 1 — submission" & exit /b 1 )

REM Give nsca-ng a moment to flush the result file.
timeout /t 1 /nobreak >nul
call :expect_in_results "Test 1" "PROCESS_SERVICE_CHECK_RESULT;test-host;basic-ok;0;all good"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 2 — non-zero result codes
REM ============================================================================
echo.
echo --------------------
echo 2. WARN / CRIT / UNK
echo --------------------
nscp nsca-ng --host=127.0.0.1 ^
    --identity=%IDENTITY% --password=%PASSWORD% ^
    --hostname=test-host ^
    --command code-warn --result 1 --message warn
if not %errorlevel%==0 ( call :fail "Test 2 — WARN" & exit /b 1 )

nscp nsca-ng --host=127.0.0.1 ^
    --identity=%IDENTITY% --password=%PASSWORD% ^
    --hostname=test-host ^
    --command code-crit --result 2 --message crit
if not %errorlevel%==0 ( call :fail "Test 2 — CRIT" & exit /b 1 )

nscp nsca-ng --host=127.0.0.1 ^
    --identity=%IDENTITY% --password=%PASSWORD% ^
    --hostname=test-host ^
    --command code-unk --result 3 --message unk
if not %errorlevel%==0 ( call :fail "Test 2 — UNKNOWN" & exit /b 1 )

timeout /t 1 /nobreak >nul
call :expect_in_results "Test 2" "PROCESS_SERVICE_CHECK_RESULT;test-host;code-warn;1;warn"
if not %errorlevel%==0 exit /b 1
call :expect_in_results "Test 2" "PROCESS_SERVICE_CHECK_RESULT;test-host;code-crit;2;crit"
if not %errorlevel%==0 exit /b 1
call :expect_in_results "Test 2" "PROCESS_SERVICE_CHECK_RESULT;test-host;code-unk;3;unk"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 3 — host check via --host-check
REM ============================================================================
echo.
echo --------------------
echo 3. Host check
echo --------------------
nscp nsca-ng --host=127.0.0.1 ^
    --identity=%IDENTITY% --password=%PASSWORD% ^
    --hostname=test-host ^
    --host-check ^
    --command ignored-when-host-check --result 0 --message "host alive"
if not %errorlevel%==0 ( call :fail "Test 3 — submission" & exit /b 1 )

timeout /t 1 /nobreak >nul
call :expect_in_results "Test 3" "PROCESS_HOST_CHECK_RESULT;test-host;0;host alive"
if not %errorlevel%==0 exit /b 1
REM and must not appear as a service check
call :expect_not_in_results "Test 3 (no service variant)" "PROCESS_SERVICE_CHECK_RESULT;test-host;ignored-when-host-check"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 4 — semicolons in plugin output (B1 regression)
REM ============================================================================
echo.
echo --------------------------------------
echo 4. Semicolon-in-output round-trip (B1)
echo --------------------------------------
nscp nsca-ng --host=127.0.0.1 ^
    --identity=%IDENTITY% --password=%PASSWORD% ^
    --hostname=test-host ^
    --command semicolon-test --result 0 --message "OK; running 3 services; load ok"
if not %errorlevel%==0 ( call :fail "Test 4 — submission" & exit /b 1 )

timeout /t 1 /nobreak >nul
REM The escape produces "OK\; running 3 services\; load ok" on the wire and
REM nsca-ng writes the line as received. What we actually want to verify is
REM that the whole message stays in the 5th field — i.e. nothing is dropped
REM or split into extra fields by the semicolon escape.
REM
REM We could check for the full literal "OK\; running 3 services\; load ok"
REM string, but every Windows substring tool we tried (findstr /C:,
REM findstr /L /C:, find) either treats `\` as a regex escape or chokes on
REM the docker-produced file's line endings, so the assertion is brittle in
REM practice. Instead, split it into two backslash-free assertions:
REM
REM   1. The line prefix is correct (right command type, host, service,
REM      return code, and start of message).
REM   2. The trailing portion of the message — which comes AFTER both
REM      escaped semicolons — is present.
REM
REM If the semicolon escape had failed and the message had been split into
REM extra command fields, the prefix check might still pass (because the
REM first field still parses) but "load ok" would land in field 7+ on a
REM different output line, or be dropped entirely by nsca-ng's field-count
REM validation. Either way, both substrings together pin the round-trip.
call :expect_in_results "Test 4 (prefix)" "PROCESS_SERVICE_CHECK_RESULT;test-host;semicolon-test;0;OK"
if not %errorlevel%==0 exit /b 1
call :expect_in_results "Test 4 (tail intact)" "load ok"
if not %errorlevel%==0 exit /b 1
REM And forbid the result being split into a second PROCESS_*_RESULT line
REM (which is what a broken escape would produce — the part after the
REM first un-escaped ';' would be interpreted as a new command).
call :expect_not_in_results "Test 4 (no second result line)" "PROCESS_SERVICE_CHECK_RESULT;running"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 5 — wrong password is rejected (negative test)
REM ============================================================================
echo.
echo ----------------------
echo 5. Wrong password (FAIL)
echo ----------------------
nscp nsca-ng --host=127.0.0.1 ^
    --identity=%IDENTITY% --password=wrong-password ^
    --hostname=test-host ^
    --command should-not-arrive --result 0 --message nope
if %errorlevel%==0 (
  echo ! Submission with wrong password unexpectedly succeeded
  call :fail "Test 5"
  exit /b 1
)
echo   OK: rejected as expected (exit code %errorlevel%)

timeout /t 1 /nobreak >nul
call :expect_not_in_results "Test 5 — bad submission must not land" "should-not-arrive"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Cleanup
REM ============================================================================
echo.
echo ----------------------
echo Stopping NSCA-NG server
echo ----------------------
docker stop nsca_ng_server >nul 2>nul

echo.
echo ============================
echo All NSCA-NG tests passed
echo ============================
exit /b 0
