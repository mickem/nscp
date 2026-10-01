#!/bin/bash

# Define a function to handle failures (equivalent to the :failed label)
fail() {
    echo "Tests failed."
    exit 1
}

echo "Running Python tests..."
nscp unit --language python --script test_python || fail

echo "All tests passed successfully."
exit 0