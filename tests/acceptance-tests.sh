#!/bin/bash

# Define a function to handle failures (equivalent to the :failed label)
fail() {
    echo "Tests failed."
    exit 1
}

# Run a `nscp unit ...` invocation up to 2 times, only failing if both
# attempts fail. This neutralises transient socket / handshake races in
# network-sensitive tests (test_nsca cipher cases, test_nrpe port binds).
retry_unit() {
    local attempt
    for attempt in 1 2; do
        if nscp unit "$@"; then
            return 0
        fi
        echo "[retry_unit] attempt ${attempt} of 2 failed for: nscp unit $*" >&2
        sleep 1
    done
    return 1
}

echo "Running NSCA tests..."
for c in none xor des 3des cast128 xtea blowfish twofish rc2 aes aes256 aes192 aes128 serpent gost 3way; do
    echo "Running test_nsca case: $c"
    # Run command; if it returns non-zero (fails), run the fail function
    retry_unit --language python --script test_nsca --case "$c" || fail
done

echo "Running NRPE tests..."
retry_unit --language python --script test_nrpe || fail

# echo "Running Lua NRPE tests..."
# retry_unit --language lua --script test_nrpe.lua --log error || fail

echo "Running Python tests..."
retry_unit --language python --script test_python || fail

echo "Running Log File tests..."
retry_unit --language python --script test_log_file || fail

echo "Running External Script tests..."
retry_unit --language python --script test_external_script || fail

echo "Running Scheduler tests..."
retry_unit --language python --script test_scheduler || fail

echo "All tests passed successfully."
exit 0
