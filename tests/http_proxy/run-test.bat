@echo off
setlocal
set script_folder=%~dp0

REM ============================================================
REM HTTP proxy acceptance tests for NSClient++
REM
REM Covers:
REM   Tests 1-4: NRDPClient sending notifications via proxy
REM   Tests 5-8: settings_http loading config via proxy (boot.ini)
REM
REM Topology:
REM   nscp (Windows host)
REM     --> squid proxy (Docker, ports 3128/3129, bridged to proxy_test_net)
REM         --> nrdp_origin (Docker, port 80, internal-network only)
REM
REM Because nrdp_origin has no port published to the host, a direct
REM connection from nscp always fails — proving that successful tests
REM really did use the proxy.
REM
REM Tests 5-8 install a temporary boot.ini next to nscp.exe configuring
REM [proxy].url; the original boot.ini is backed up and restored under
REM the :cleanup label even if a test fails.
REM ============================================================

echo ==================================================================
echo Building Docker images
echo ==================================================================

docker build -t nrdp_proxy   -f %script_folder%Dockerfile        %script_folder%
if not %errorlevel%==0 (
    echo ! Failed to build nrdp_proxy image
    exit /b 1
)

docker build -t nrdp_origin  -f %script_folder%Dockerfile.origin %script_folder%
if not %errorlevel%==0 (
    echo ! Failed to build nrdp_origin image
    exit /b 1
)

echo ==================================================================
echo Creating isolated Docker network
echo ==================================================================

docker network create proxy_test_net 2>nul

echo ==================================================================
echo Starting containers
echo ==================================================================

REM Origin: NOT published to the host - reachable only through the proxy
docker run -d --rm --network proxy_test_net --name nrdp_origin nrdp_origin
if not %errorlevel%==0 (
    echo ! Failed to start nrdp_origin container
    docker network rm proxy_test_net 2>nul
    exit /b 1
)

REM Proxy: published on 3128 (open) and 3129 (authenticated)
docker run -d --rm --network proxy_test_net -p 3128:3128 -p 3129:3129 --name nrdp_proxy nrdp_proxy
if not %errorlevel%==0 (
    echo ! Failed to start nrdp_proxy container
    docker stop nrdp_origin 2>nul
    docker network rm proxy_test_net 2>nul
    exit /b 1
)

echo Waiting for containers to initialise...
timeout /t 5 /nobreak >nul

echo ==================================================================
echo Test 1: Plain HTTP through open proxy (no authentication)
echo ==================================================================
nscp nrdp --host nrdp_origin --port 80 --token mytoken --command proxy_test --message "HTTP proxy test" --result 0 --proxy http://127.0.0.1:3128/
if not %errorlevel%==0 (
    echo ! Test 1 FAILED: expected success
    docker stop nrdp_origin nrdp_proxy 2>nul
    docker network rm proxy_test_net 2>nul
    exit /b 1
)
echo + Test 1 passed

echo ==================================================================
echo Test 2: Plain HTTP through authenticated proxy (correct credentials)
echo ==================================================================
nscp nrdp --host nrdp_origin --port 80 --token mytoken --command proxy_test --message "HTTP proxy test" --result 0 --proxy http://testuser:testpass@127.0.0.1:3129/
if not %errorlevel%==0 (
    echo ! Test 2 FAILED: expected success
    docker stop nrdp_origin nrdp_proxy 2>nul
    docker network rm proxy_test_net 2>nul
    exit /b 1
)
echo + Test 2 passed

echo ==================================================================
echo Test 3: Plain HTTP through authenticated proxy (wrong credentials)
echo ==================================================================
nscp nrdp --host nrdp_origin --port 80 --token mytoken --command proxy_test --message "HTTP proxy test" --result 0 --proxy http://testuser:wrongpass@127.0.0.1:3129/
if %errorlevel%==0 (
    echo ! Test 3 FAILED: expected failure ^(407 auth^) but got success
    docker stop nrdp_origin nrdp_proxy 2>nul
    docker network rm proxy_test_net 2>nul
    exit /b 1
)
echo + Test 3 passed

echo ==================================================================
echo Test 4: no-proxy bypass - origin unreachable without the proxy
echo ==================================================================
REM nrdp_origin is in no-proxy, so nscp skips the proxy and tries a direct
REM TCP connection.  Because nrdp_origin has no published port on the host,
REM the connection attempt fails - proving the bypass logic is active.
nscp nrdp --host nrdp_origin --port 80 --token mytoken --command proxy_test --message "HTTP proxy test" --result 0 --proxy http://127.0.0.1:3128/ --no-proxy nrdp_origin
if %errorlevel%==0 (
    echo ! Test 4 FAILED: expected failure ^(bypass should prevent proxy use^) but got success
    docker stop nrdp_origin nrdp_proxy 2>nul
    docker network rm proxy_test_net 2>nul
    exit /b 1
)
echo + Test 4 passed

echo ==================================================================
echo Locating nscp.exe and preparing boot.ini for settings tests
echo ==================================================================
REM ------------------------------------------------------------
REM Settings-via-proxy tests use a temporary boot.ini placed next
REM to nscp.exe.  Any existing boot.ini is backed up and restored
REM in the cleanup section below — failures must goto :cleanup so
REM the original boot.ini is always put back.
REM
REM `nscp settings --validate` and `--show` always exit 0 even when
REM settings_http silently fails to download (it falls through to an
REM empty INI child).  Instead of relying on the exit code, each test
REM fetches a unique URL whose served INI carries a known marker key
REM /settings/test/proxy_marker=ok and greps stdout of `--show` for
REM that marker.  Per-test URL paths keep cache files separate so a
REM passing test cannot mask a later failing one.
REM ------------------------------------------------------------
set NSCP_EXE=
for /f "delims=" %%i in ('where nscp 2^>nul') do (
    if not defined NSCP_EXE set NSCP_EXE=%%i
)
if "%NSCP_EXE%"=="" (
    echo ! Cannot find nscp.exe in PATH; skipping HTTP-settings tests
    goto :docker_cleanup
)
for %%i in ("%NSCP_EXE%") do set NSCP_DIR=%%~dpi
set BOOT_INI=%NSCP_DIR%boot.ini
set BOOT_INI_BACKUP=%NSCP_DIR%boot.ini.proxytest.bak
set NSCP_OUT=%TEMP%\nscp_proxy_test_out.txt
echo Using nscp at: %NSCP_EXE%
echo Boot.ini path: %BOOT_INI%

if exist "%BOOT_INI%" (
    copy /Y "%BOOT_INI%" "%BOOT_INI_BACKUP%" >nul
    if not %errorlevel%==0 (
        echo ! Failed to backup existing boot.ini
        goto :docker_cleanup
    )
)

set TEST_FAILED=0

echo ==================================================================
echo Test 5: HTTP settings load through open proxy
echo ==================================================================
> "%BOOT_INI%" (
    echo [proxy]
    echo url = http://127.0.0.1:3128/
)
nscp settings --show --path /settings/test --key proxy_marker --settings http://nrdp_origin/settings_t5.ini > "%NSCP_OUT%" 2>nul
findstr /C:"ok" "%NSCP_OUT%" >nul
if not %errorlevel%==0 (
    echo ! Test 5 FAILED: marker not found ^(download via proxy failed^)
    type "%NSCP_OUT%"
    set TEST_FAILED=1
    goto :cleanup
)
echo + Test 5 passed

echo ==================================================================
echo Test 6: HTTP settings load through authenticated proxy (correct creds)
echo ==================================================================
> "%BOOT_INI%" (
    echo [proxy]
    echo url = http://testuser:testpass@127.0.0.1:3129/
)
nscp settings --show --path /settings/test --key proxy_marker --settings http://nrdp_origin/settings_t6.ini > "%NSCP_OUT%" 2>nul
findstr /C:"ok" "%NSCP_OUT%" >nul
if not %errorlevel%==0 (
    echo ! Test 6 FAILED: marker not found ^(authenticated proxy fetch failed^)
    type "%NSCP_OUT%"
    set TEST_FAILED=1
    goto :cleanup
)
echo + Test 6 passed

echo ==================================================================
echo Test 7: HTTP settings load through authenticated proxy (wrong creds)
echo ==================================================================
> "%BOOT_INI%" (
    echo [proxy]
    echo url = http://testuser:wrongpass@127.0.0.1:3129/
)
REM Wrong creds → proxy returns 407 → settings_http loads no keys →
REM the marker should NOT appear in stdout.
nscp settings --show --path /settings/test --key proxy_marker --settings http://nrdp_origin/settings_t7.ini > "%NSCP_OUT%" 2>nul
findstr /C:"ok" "%NSCP_OUT%" >nul
if %errorlevel%==0 (
    echo ! Test 7 FAILED: marker present despite wrong credentials
    type "%NSCP_OUT%"
    set TEST_FAILED=1
    goto :cleanup
)
echo + Test 7 passed

echo ==================================================================
echo Test 8: HTTP settings no-proxy bypass (origin unreachable directly)
echo ==================================================================
> "%BOOT_INI%" (
    echo [proxy]
    echo url = http://127.0.0.1:3128/
    echo no_proxy = nrdp_origin
)
REM nrdp_origin is in no_proxy, so nscp tries a direct TCP connect.
REM nrdp_origin is not published on the host, so the connect fails —
REM the marker must NOT appear, proving the bypass skipped the proxy.
nscp settings --show --path /settings/test --key proxy_marker --settings http://nrdp_origin/settings_t8.ini > "%NSCP_OUT%" 2>nul
findstr /C:"ok" "%NSCP_OUT%" >nul
if %errorlevel%==0 (
    echo ! Test 8 FAILED: marker present despite bypass ^(proxy should have been skipped^)
    type "%NSCP_OUT%"
    set TEST_FAILED=1
    goto :cleanup
)
echo + Test 8 passed

:cleanup
echo ==================================================================
echo Restoring boot.ini
echo ==================================================================
if exist "%BOOT_INI_BACKUP%" (
    move /Y "%BOOT_INI_BACKUP%" "%BOOT_INI%" >nul
) else (
    if exist "%BOOT_INI%" del /Q "%BOOT_INI%"
)
if defined NSCP_OUT if exist "%NSCP_OUT%" del /Q "%NSCP_OUT%" 2>nul

:docker_cleanup
echo ==================================================================
echo Cleaning up
echo ==================================================================
docker stop nrdp_origin nrdp_proxy 2>nul
docker network rm proxy_test_net 2>nul

if "%TEST_FAILED%"=="1" exit /b 1

echo ==================================================================
echo All HTTP proxy tests completed successfully
echo ==================================================================
exit /b 0
