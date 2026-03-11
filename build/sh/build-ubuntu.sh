#!/bin/bash
set -e

PACKAGE_NAME="nsclient-server"
BUILD_DIR="cmake-build-package"
OUTPUT_DIR="package_output"

echo "--- [1/5] Cleaning previous environment ---"
# Remove previous build artifacts
# rm -rf $BUILD_DIR $OUTPUT_DIR
# Ensure the package is not currently installed
if dpkg -l | grep -q "^ii  $PACKAGE_NAME"; then
    echo "Removing existing installation of $PACKAGE_NAME..."
    sudo apt purge -y $PACKAGE_NAME
fi

echo "--- [2/5] Building Project via CMake ---"
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake .. -DCMAKE_BUILD_TYPE=Release
#cmake --build . --target package
make package_source

# Move the zip to a clean staging area to simulate the 'release'
mkdir -p ../$OUTPUT_DIR/source-root
cp *.tar.gz ../$OUTPUT_DIR/
cd ../$OUTPUT_DIR/source-root

echo "--- [3/5] Building .deb Package ---"
tar zxvf ../*.tar.gz
cd *
cp -r ../../debian ./

dpkg-buildpackage -us -uc -b

cd ..
DEB_FILE=$(ls *.deb | head -n 1)

echo "--- [4/5] Static Validation (Lintian) ---"
# Lintian checks for policy violations
lintian "$DEB_FILE" --no-tag-display-limit
if [ $? -eq 0 ]; then
    echo "Lintian check passed."
else
    echo "WARNING: Lintian found issues."
fi

echo "--- [5/5] Install & Runtime Validation ---"
sudo apt install -y "./$DEB_FILE"

if which your_binary_name > /dev/null; then
    echo "Binary found in path."
else
    echo "ERROR: Binary not found after install."
    exit 1
fi

if your_binary_name --version; then
    echo "Runtime test passed!"
else
    echo "ERROR: Runtime execution failed."
    exit 1
fi

echo "--- SUCCESS: Package built, installed, and validated correctly. ---"