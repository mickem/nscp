#!/bin/bash

# Define a function to handle failures (equivalent to the :failed label)
fail() {
    echo "Tests failed."
    exit 1
}

echo "Running NSCA tests..."
for c in none xor des 3des cast128 xtea blowfish twofish rc2 aes aes256 aes192 aes128 serpent gost 3way; do
    echo "Running test_nsca case: $c"
    # Run command; if it returns non-zero (fails), run the fail function
    nscp unit --language python --script test_nsca --case "$c" || fail
done

echo "Running NRPE tests..."
nscp unit --language python --script test_nrpe || fail

# echo "Running Lua NRPE tests..."
# nscp unit --language lua --script test_nrpe.lua --log error || fail

echo "Running Python tests..."
nscp unit --language python --script test_python || fail

echo "Running Log File tests..."
nscp unit --language python --script test_log_file || fail

echo "Running External Script tests..."
nscp unit --language python --script test_external_script || fail

echo "Running CheckHelpers tests..."
nscp unit --language python --script test_check_helpers || fail

echo "Running Scheduler tests..."
nscp unit --language python --script test_scheduler || fail

# check_http with no ca= of its own falls back to ${ca-path}, the platform CA
# bundle that CheckNet::loadModuleEx resolves at load. That default is a single
# hardcoded path (service/path_manager.cpp) and it is the Debian/Ubuntu one on
# every non-Windows platform, so on RHEL-family - where the bundle is
# /etc/pki/tls/certs/ca-bundle.crt - this fails before a socket is opened:
#
#   CRITICAL: ... error: Failed to load CA /etc/ssl/certs/ca-certificates.crt
#
# This lives here rather than in the jest suite because the jest steps only run
# for DEB packages (integration-tests-linux.yml), so nothing else in CI exercises
# the default bundle on the distribution where it is wrong. Needs egress; it is
# the only test here that does.
echo "Running CheckNet platform-trust-store test..."
http_out=$(nscp client --module CheckNet --boot --query check_http url=https://www.google.com critical="code != 200" 2>&1 | tail -1)
echo "  $http_out"
case "$http_out" in
    OK:*) ;;
    *)
        echo "check_http could not validate a public HTTPS site using the platform CA bundle."
        fail
        ;;
esac

echo "All tests passed successfully."
exit 0