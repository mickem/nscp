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

echo "Running Scheduler tests..."
nscp unit --language python --script test_scheduler || fail

echo "All tests passed successfully."
exit 0