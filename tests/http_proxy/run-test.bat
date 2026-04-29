@echo off
setlocal
set script_folder=%~dp0

REM ============================================================
REM HTTP proxy acceptance tests for NSClient++ (NRDPClient)
REM
REM Topology:
REM   nscp (Windows host)
REM     --> squid proxy (Docker, ports 3128/3129, bridged to proxy_test_net)
REM         --> nrdp_origin (Docker, port 80, internal-network only)
REM
REM Because nrdp_origin has no port published to the host, a direct
REM connection from nscp always fails — proving that successful tests
REM really did use the proxy.
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
echo Cleaning up
echo ==================================================================
docker stop nrdp_origin nrdp_proxy 2>nul
docker network rm proxy_test_net 2>nul

echo ==================================================================
echo All HTTP proxy tests completed successfully
echo ==================================================================
exit /b 0
