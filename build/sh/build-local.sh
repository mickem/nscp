#/bin/bash

cd /src/build
cmake $* /src/nscp || exit 1
make || exit 1
make test|| exit 1
