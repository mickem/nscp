@echo off
set script_folder=%~dp0

mkdir nrpe_test
cd nrpe_test

set OPENSSL_CONF=%script_folder%openssl.cnf
echo ------------------
echo Create the Root CA
echo ------------------

echo - Generating CA private key...
openssl genpkey -algorithm RSA -out ca.key -pkeyopt rsa_keygen_bits:2048
echo - Generating self-signed CA certificate...
openssl req -x509 -new -key ca.key -out ca.crt -days 365 -subj "/CN=root-ca" -config %OPENSSL_CONF%
echo - CA certificate (ca.crt) and key (ca.key) created.

echo -------------------------------
echo Creating the Server Certificate
echo -------------------------------

echo - Generating server private key...
openssl genpkey -algorithm RSA -out server.key -pkeyopt rsa_keygen_bits:2048
echo - Generating server Certificate Signing Request (CSR)...
openssl req -new -key server.key -out server.csr -subj "/CN=localhost"
echo - Signing server CSR with CA key...
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -out server.crt -days 365 -set_serial 01
echo - Server certificate (server.crt) and key (server.key) created.

echo -------------------------------
echo Creating the Client Certificate
echo -------------------------------
echo - Generating client private key...
openssl genpkey -algorithm RSA -out client.key -pkeyopt rsa_keygen_bits:2048
echo - Generating client Certificate Signing Request (CSR)...
openssl req -new -key client.key -out client.csr -subj /CN=localhost
echo - Signing client CSR with CA key...
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -out client.crt -days 365 -set_serial 02
echo - Client certificate (client.crt) and key (client.key) created.

echo -----------------------------------------------------------
echo All certificates and keys have been generated successfully.
echo -----------------------------------------------------------
cd ..
docker build -t check_nrpe -f %script_folder%Dockerfile nrpe_test
if not %errorlevel%==0 (
  echo ! Failed to build check_nrpe docker image, Error level was: %errorlevel%
  exit /b 1
)

echo ------------------------------
echo Running unencrypted NRPE test
echo ------------------------------
nscp nrpe install --allowed-hosts 127.0.0.1 --insecure
nscp lua install
nscp lua add --script mock
start nscp test

timeout /t 3 /nobreak >nul

echo - Testing without TLS...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 -c mock_query
if not %errorlevel%==0 (
  echo ! Failed to connect with NRPE, Error level was: %errorlevel%
  exit /b 1
)
echo - Shutting down server...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 -c mock_exit
if not %errorlevel%==0 (
  echo ! Failed to exit via NRPE, Error level was: %errorlevel%
  exit /b 1
)

timeout /t 3 /nobreak >nul


echo ------------------------------------------------
echo Running 1-way TLS with no cert verification test
echo ------------------------------------------------
nscp nrpe install --allowed-hosts 127.0.0.1 --insecure=false --verify=none --certificate nrpe_test\server.crt --certificate-key nrpe_test\server.key
nscp lua install
nscp lua add --script mock
start nscp test

timeout /t 3 /nobreak >nul

echo - Testing without client certificate...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 --ssl-version TLSv1.2+ -c mock_query
if not %errorlevel%==0 (
  echo ! Failed to connect with NRPE, Error level was: %errorlevel%
  exit /b 1
)
echo - Testing invalid TLS version...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 --ssl-version SSLv3 --client-cert /test/client.crt --key-file /test/client.key -c mock_query
if not %errorlevel%==2 (
  echo ! Succeeded to connect with invalid TLS version, Error level was: %errorlevel%
  exit /b 1
)
echo - Shutting down server...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 --ssl-version TLSv1.2+ -c mock_exit
if not %errorlevel%==0 (
  echo ! Failed to exit via NRPE, Error level was: %errorlevel%
  exit /b 1
)

timeout /t 3 /nobreak >nul

echo ----------------------
echo Running 2-way TLS test
echo ----------------------
nscp nrpe install --allowed-hosts 127.0.0.1 --insecure=false --verify=peer-cert --certificate nrpe_test\server.crt --certificate-key nrpe_test\server.key --ca nrpe_test\ca.crt
nscp lua install
nscp lua add --script mock
start nscp test

timeout /t 3 /nobreak >nul

echo - Testing valid client certificate...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 --ssl-version TLSv1.2+ --client-cert /test/client.crt --key-file /test/client.key -c mock_query
if not %errorlevel%==0 (
  echo ! Failed to connect with NRPE, Error level was: %errorlevel%
  exit /b 1
)
echo - Testing without certificate...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 -c mock_query
if not %errorlevel%==3 (
  echo ! Succeeded to connect with invalid certificate, Error level was: %errorlevel%
  exit /b 1
)
echo - Testing invalid TLS version...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 --ssl-version SSLv3 --client-cert /test/client.crt --key-file /test/client.key -c mock_query
if not %errorlevel%==2 (
  echo ! Succeeded to connect with invalid TLS version, Error level was: %errorlevel%
  exit /b 1
)
echo - Shutting down server...
docker run --rm check_nrpe check_nrpe -H host.docker.internal -p 5666 -t5 --ssl-version TLSv1.2+ --client-cert /test/client.crt --key-file /test/client.key -c mock_exit
if not %errorlevel%==0 (
  echo ! Failed to exit via NRPE, Error level was: %errorlevel%
  exit /b 1
)

echo --------------------------------
echo All tests completed successfully
echo --------------------------------









