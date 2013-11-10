function test {
    "$@" | tee build.log
    status=$?
    if [ $status -ne 0 ]; then
        echo "Error with $1 from $@"
    fi
    return $status
}

test mkdir build
test pushd build
test cmake /source/nscp
test make
test ctest --output-on-failure
