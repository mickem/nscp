@echo off
REM ============================================================================
REM tests/icinga-client/run-test.bat
REM
REM Builds Icinga2's check_nscp_api in a docker container (see Dockerfile)
REM and runs it against this build's WEBServer over HTTPS. Mirrors the
REM tests/nrpe pattern: NSClient runs on the host, check_nscp_api runs from
REM the container and reaches the host via host.docker.internal.
REM
REM What this test covers:
REM   1. Plain happy-path query — check_nscp_api with the correct password
REM      gets a 200 from /query/<command>.
REM   2. Wrong password rejected (403).
REM   3. Icinga User-Agent allowlist — check_nscp_api in older releases
REM      sends ?password=... in the query string. We do NOT use the legacy
REM      query-string vector in this test (current check_nscp_api uses a
REM      header), but the User-Agent matches the default allowlist so any
REM      future Icinga release that re-introduces query-string auth keeps
REM      working out of the box. The auth.test.ts REST suite covers the
REM      allowlist mechanics; here we just confirm the real Icinga binary
REM      authenticates successfully with NSClient's default config.
REM ============================================================================

set script_folder=%~dp0
set current_dir=%cd%

if exist icinga_client_test rmdir /s /q icinga_client_test
mkdir icinga_client_test

echo ------------------
echo Building container
echo ------------------
REM Two Dockerfiles are available:
REM   * Dockerfile      — builds check_nscp_api from pinned upstream source.
REM                       Reproducible, ~5-8 min cold build.
REM   * Dockerfile.deb  — installs `icinga2-bin` from Debian. Seconds to
REM                       build; version drifts with the base image.
REM
REM The deb variant is the default because the test only cares that an
REM Icinga-built check_nscp_api can authenticate against this server —
REM behaviour-pinning to a specific Icinga commit is overkill here. Set
REM DOCKERFILE=Dockerfile (or pass it as an env var) when you want a
REM reproducible source build, e.g. to bisect an Icinga regression.
if "%DOCKERFILE%"=="" set DOCKERFILE=Dockerfile.deb
echo Using %DOCKERFILE%
docker build -t check_nscp_api -f %script_folder%%DOCKERFILE% %script_folder%
if not %errorlevel%==0 (
  echo ! Failed to build check_nscp_api docker image, error level: %errorlevel%
  exit /b 1
)

echo ----------------------------
echo Configuring NSClient WEBServer
echo ----------------------------
REM Use a known admin password so the test can authenticate predictably.
REM `web install` writes a fresh /settings/WEB/server section with admin's
REM password set to the supplied value. The valid options are visible via
REM `nscp web install --help`: allowed-hosts, certificate, certificate-key,
REM port, password, https. (--insecure does not exist on this subcommand;
REM there is no insecure mode for the WEB server.)
nscp web install --password test-password --allowed-hosts "127.0.0.1,0.0.0.0/0"
nscp settings --activate-module CheckHelpers
REM Open allowed-hosts up to the Docker bridge so the container can reach
REM 8443. 0.0.0.0/0 is fine for a local test run.
nscp lua install
nscp lua add --script mock

echo --------------------------
echo Starting NSClient (nscp test)
echo --------------------------
start nscp test

REM Poll for the WEB server to bind 8443 rather than guessing with a fixed
REM sleep. nscp test takes longer to start on cold caches or slow disks; a
REM hard-coded 5s loses the race intermittently. Give up after ~30s — any
REM longer than that is a real failure, not a slow boot.
echo Waiting for nscp WEB server on 127.0.0.1:8443 ...
powershell -NoProfile -Command ^
    "for ($i=0; $i -lt 30; $i++) {" ^
    "  try { $c=New-Object System.Net.Sockets.TcpClient;" ^
    "        $c.Connect('127.0.0.1',8443); $c.Close(); exit 0 }" ^
    "  catch { Start-Sleep -Seconds 1 } };" ^
    "exit 1"
if not %errorlevel%==0 (
  echo ! nscp WEB server did not bind 8443 within 30s
  goto :failed
)
echo nscp is ready.

echo ----------------------------------------
echo 1. check_nscp_api — valid password
echo ----------------------------------------
docker run --rm check_nscp_api ^
    --host host.docker.internal ^
    --port 8443 ^
    --password test-password ^
    --query mock_query ^
    > icinga_client_test\out.txt 2>&1
call :assert_ok "valid-password" "mock_query::" 0 %errorlevel%
if errorlevel 1 goto :failed

echo ----------------------------------------
echo 2. check_nscp_api — invalid password rejected
echo ----------------------------------------
docker run --rm check_nscp_api ^
    --host host.docker.internal ^
    --port 8443 ^
    --password definitely-not-the-password ^
    --query mock_query ^
    > icinga_client_test\out.txt 2>&1
REM check_nscp_api maps auth failure to UNKNOWN (3) or CRITICAL (2); accept
REM either as long as it's a non-zero failure code.
if "%errorlevel%"=="0" (
  echo ! Wrong password unexpectedly accepted
  type icinga_client_test\out.txt
  goto :failed
)
echo   OK: rejected (exit %errorlevel%)

echo ----------------------------------------
echo 3. check_nscp_api — warning result propagates
echo ----------------------------------------
docker run --rm check_nscp_api ^
    --host host.docker.internal ^
    --port 8443 ^
    --password test-password ^
    --query check_always_warning ^
    --arguments check_ok "message=warn-from-icinga" ^
    > icinga_client_test\out.txt 2>&1
REM check_nscp_api exits with the Nagios code (0/1/2/3) it parses from the
REM response. We told mock to return WARNING.
if not "%errorlevel%"=="1" (
  echo ! Expected WARNING exit code 1, got %errorlevel%
  type icinga_client_test\out.txt
  goto :failed
)
findstr /c:"warn-from-icinga" icinga_client_test\out.txt >nul
if errorlevel 1 (
  echo ! Output did not echo the supplied message
  type icinga_client_test\out.txt
  goto :failed
)
echo   OK: WARNING propagated and message echoed

echo ----------------------------------------
echo Shutting down nscp test
echo ----------------------------------------
REM nscp test has no clean-shutdown CLI from outside; the simplest portable
REM way is to kill the process tree by image name.
taskkill /F /IM nscp.exe /T >nul 2>&1

echo --------------------------------
echo All tests completed successfully
echo --------------------------------
exit /b 0

:failed
taskkill /F /IM nscp.exe /T >nul 2>&1
echo -----------------
echo Tests failed
echo -----------------
exit /b 1


REM ---------------------------------------------------------------------------
REM assert_ok LABEL EXPECTED_SUBSTRING EXPECTED_EXIT ACTUAL_EXIT
REM
REM Mirrors the helper in tests/nrpe/run-test.bat. Validates the captured
REM exit code and looks for an expected substring in
REM icinga_client_test\out.txt. Returns errorlevel 0 silently on pass,
REM dumps the captured output on fail.
REM ---------------------------------------------------------------------------
:assert_ok
if not "%~4"=="%~3" (
  echo ! [%~1] expected exit code %~3 got %~4
  echo --- captured output ---
  type icinga_client_test\out.txt
  echo --- end output ---
  exit /b 1
)
findstr /c:"%~2" icinga_client_test\out.txt >nul
if errorlevel 1 (
  echo ! [%~1] output did not contain "%~2"
  echo --- captured output ---
  type icinga_client_test\out.txt
  echo --- end output ---
  exit /b 1
)
exit /b 0
