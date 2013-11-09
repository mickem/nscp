function test {
    "$@"
    status=$?
    if [ $status -ne 0 ]; then
        echo "error with $1"
    fi
    return $status
}

test git clone --recursive https://github.com/mickem/nscp.git
test pushd build
test cmake ../nscp
test make
test ctest --output-on-failure
