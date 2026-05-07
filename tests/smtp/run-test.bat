@echo off
REM ----------------------------------------------------------------------------
REM Integration test for the SMTPClient module.
REM
REM Brings up a Python aiosmtpd-based test SMTP server in Docker that:
REM   * listens plain + STARTTLS on :1025
REM   * listens implicit-TLS on :1465
REM   * accepts AUTH LOGIN / AUTH PLAIN with hardcoded credentials
REM   * captures every accepted message to nscp_smtp_test/messages.txt
REM
REM Tests cover:
REM   1. plain SMTP, no auth (security=none)
REM   2. STARTTLS + AUTH LOGIN (security=starttls)
REM   3. implicit TLS + AUTH PLAIN (security=tls)
REM   4. credentials configured but security=none -> client refuses
REM   5. wrong password -> client surfaces failure
REM   6. CRLF injection in subject -> stripped, no extra header injected
REM
REM Requirements: Docker Desktop on Windows, NSClient++ on PATH.
REM ----------------------------------------------------------------------------

set script_folder=%~dp0
set current_dir=%cd%
set USERNAME=alerts@example.com
set PASSWORD=change_me

goto :start

REM ----- helpers --------------------------------------------------------------

:fail
echo.
echo ! TEST FAILED at step: %1
echo.
echo ----- docker logs smtp_test_server (last 200 lines) -----
docker logs --tail 200 smtp_test_server 2>&1
echo ---------------------------------------------------------
docker stop smtp_test_server >nul 2>nul
exit /b 1

:expect_in_inbox
REM %1 = description, %2 = literal substring expected in messages.txt
findstr /c:%2 %current_dir%\nscp_smtp_test\messages.txt >nul
if not %errorlevel%==0 (
  echo ! Expected substring not found: %2
  echo --- messages.txt ---
  type %current_dir%\nscp_smtp_test\messages.txt
  echo --------------------
  call :fail "%~1"
  exit /b 1
)
echo   OK: found %2
goto :eof

:expect_not_in_inbox
REM %1 = description, %2 = literal substring that must NOT appear
findstr /c:%2 %current_dir%\nscp_smtp_test\messages.txt >nul
if %errorlevel%==0 (
  echo ! Forbidden substring present: %2
  echo --- messages.txt ---
  type %current_dir%\nscp_smtp_test\messages.txt
  echo --------------------
  call :fail "%~1"
  exit /b 1
)
echo   OK: absent %2
goto :eof

REM ----- main -----------------------------------------------------------------

:start

if exist nscp_smtp_test rmdir /s /q nscp_smtp_test
mkdir nscp_smtp_test

echo ------------------
echo Building container
echo ------------------

docker build -t smtp_test_server -f %script_folder%Dockerfile %script_folder%
if not %errorlevel%==0 (
  echo ! Failed to build smtp_test_server docker image, error level: %errorlevel%
  exit /b 1
)

REM Pre-create the inbox file so docker bind-mounts it as a directory
REM rather than letting the daemon decide.
echo. > %current_dir%\nscp_smtp_test\messages.txt

echo ------------------------
echo Starting SMTP test server
echo ------------------------

docker run -d --rm ^
    --volume %current_dir%\nscp_smtp_test:/inbox ^
    -p 1025:1025 -p 1465:1465 ^
    -e SMTP_USERNAME=%USERNAME% ^
    -e SMTP_PASSWORD=%PASSWORD% ^
    --name smtp_test_server smtp_test_server
if not %errorlevel%==0 ( call :fail "docker run" & exit /b 1 )

echo Waiting for SMTP test server to listen on 127.0.0.1:1025 ...
powershell -NoProfile -Command "$ok=$false; for($i=0;$i -lt 30;$i++){ try { $c=New-Object System.Net.Sockets.TcpClient; $c.Connect('127.0.0.1',1025); $c.Close(); $ok=$true; break } catch { Start-Sleep -Milliseconds 500 } }; if(-not $ok){ exit 1 }"
if not %errorlevel%==0 (
  echo ! SMTP server never started listening on 1025. Container logs:
  echo ----- docker logs smtp_test_server -----
  docker logs smtp_test_server
  echo ----------------------------------------
  call :fail "server startup"
  exit /b 1
)
echo   OK: SMTP server is accepting connections.

REM ============================================================================
REM Test 1 - plain SMTP, no auth, no TLS
REM ============================================================================
echo.
echo ----------------------------
echo 1. Plain SMTP, no auth (none)
echo ----------------------------
nscp smtp --host=127.0.0.1 --port=1025 ^
    --security=none ^
    --sender=plain@example.com ^
    --recipient=ops@example.com ^
    --subject "T1 plain" ^
    --template "T1-body %%message%%" ^
    --command t1 --result 0 --message "T1-msg"
if not %errorlevel%==0 ( call :fail "Test 1 - submission" & exit /b 1 )

timeout /t 1 /nobreak >nul
call :expect_in_inbox "Test 1" "Subject: T1 plain"
if not %errorlevel%==0 exit /b 1
call :expect_in_inbox "Test 1" "T1-body T1-msg"
if not %errorlevel%==0 exit /b 1
call :expect_in_inbox "Test 1" "MAIL_FROM=plain@example.com"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 2 - STARTTLS + AUTH (security=starttls)
REM ============================================================================
echo.
echo --------------------------------
echo 2. STARTTLS + AUTH (port 1025)
echo --------------------------------
nscp smtp --host=127.0.0.1 --port=1025 ^
    --security=starttls ^
    --insecure-skip-verify ^
    --username=%USERNAME% --password=%PASSWORD% ^
    --sender=alerts@example.com ^
    --recipient=ops@example.com ^
    --subject "T2 starttls" ^
    --template "T2-body %%message%%" ^
    --command t2 --result 0 --message "T2-msg"
if not %errorlevel%==0 ( call :fail "Test 2 - submission" & exit /b 1 )

timeout /t 1 /nobreak >nul
call :expect_in_inbox "Test 2" "Subject: T2 starttls"
if not %errorlevel%==0 exit /b 1
call :expect_in_inbox "Test 2" "T2-body T2-msg"
if not %errorlevel%==0 exit /b 1
REM AUTH=True in the captured envelope proves the auth round trip ran
call :expect_in_inbox "Test 2 (AUTH proof)" "AUTH=True"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 3 - implicit TLS + AUTH (security=tls, port 1465)
REM ============================================================================
echo.
echo ----------------------------------
echo 3. Implicit TLS + AUTH (port 1465)
echo ----------------------------------
nscp smtp --host=127.0.0.1 --port=1465 ^
    --security=tls ^
    --insecure-skip-verify ^
    --username=%USERNAME% --password=%PASSWORD% ^
    --sender=alerts@example.com ^
    --recipient=ops@example.com ^
    --subject "T3 implicit-tls" ^
    --template "T3-body %%message%%" ^
    --command t3 --result 0 --message "T3-msg"
if not %errorlevel%==0 ( call :fail "Test 3 - submission" & exit /b 1 )

timeout /t 1 /nobreak >nul
call :expect_in_inbox "Test 3" "Subject: T3 implicit-tls"
if not %errorlevel%==0 exit /b 1
call :expect_in_inbox "Test 3" "T3-body T3-msg"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 4 - credentials with security=none must be refused (FAIL expected)
REM ============================================================================
echo.
echo ---------------------------------------------
echo 4. Credentials with security=none (must FAIL)
echo ---------------------------------------------
nscp smtp --host=127.0.0.1 --port=1025 ^
    --security=none ^
    --username=%USERNAME% --password=%PASSWORD% ^
    --sender=alerts@example.com ^
    --recipient=ops@example.com ^
    --subject "T4 should-not-arrive" ^
    --template "should-not-arrive" ^
    --command t4 --result 0 --message "T4-msg"
if %errorlevel%==0 (
  echo ! Submission with security=none and credentials unexpectedly succeeded
  call :fail "Test 4"
  exit /b 1
)
echo   OK: refused as expected (exit code %errorlevel%)
timeout /t 1 /nobreak >nul
call :expect_not_in_inbox "Test 4 (clear-text auth attempt must not land)" "T4 should-not-arrive"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 5 - wrong password (FAIL expected)
REM ============================================================================
echo.
echo ----------------------------
echo 5. Wrong password (must FAIL)
echo ----------------------------
nscp smtp --host=127.0.0.1 --port=1025 ^
    --security=starttls ^
    --insecure-skip-verify ^
    --username=%USERNAME% --password=wrong-password ^
    --sender=alerts@example.com ^
    --recipient=ops@example.com ^
    --subject "T5 wrong-password" ^
    --template "should-not-arrive" ^
    --command t5 --result 0 --message "T5-msg"
if %errorlevel%==0 (
  echo ! Submission with wrong password unexpectedly succeeded
  call :fail "Test 5"
  exit /b 1
)
echo   OK: rejected as expected (exit code %errorlevel%)
timeout /t 1 /nobreak >nul
call :expect_not_in_inbox "Test 5 (bad auth must not land)" "T5 wrong-password"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Test 6 - CRLF injection in subject (must be stripped)
REM ============================================================================
echo.
echo --------------------------------------
echo 6. CRLF injection in subject (sanitised)
echo --------------------------------------
REM Use powershell -Command to construct a string that actually contains
REM \r\n at the OS level - cmd's `\n` is a literal backslash+n.
powershell -NoProfile -Command "& nscp smtp --host=127.0.0.1 --port=1025 --security=starttls --insecure-skip-verify --username='%USERNAME%' --password='%PASSWORD%' --sender=alerts@example.com --recipient=ops@example.com --subject ('hi' + [char]13 + [char]10 + 'Bcc: evil@example.com') --template T6-body --command t6 --result 0 --message T6-msg"
if not %errorlevel%==0 ( call :fail "Test 6 - submission" & exit /b 1 )

timeout /t 1 /nobreak >nul
REM The subject body should be present (sanitised to "hiBcc: evil...") but
REM there must NOT be a real Bcc header on its own line.
call :expect_not_in_inbox "Test 6 (no injected Bcc header)" "RCPT_TO=evil@example.com"
if not %errorlevel%==0 exit /b 1

REM ============================================================================
REM Cleanup
REM ============================================================================
echo.
echo ----------------------
echo Stopping SMTP server
echo ----------------------
docker stop smtp_test_server >nul 2>nul

echo.
echo ============================
echo All SMTP tests passed
echo ============================
exit /b 0
