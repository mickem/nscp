#!/usr/bin/env bash
#
# Verify that a dependency cloned from git sits on the commit recorded for it in
# .github/dependency-checksums.txt. A tag is a mutable pointer - the owner of
# the repository, or anyone who takes it over, can move it to different code
# without the build noticing - so a dependency the build compiles and links has
# to be pinned to the immutable commit id instead.
#
#   .github/scripts/verify-commit.sh googletest 1.12.1 tmp/googletest
#
# Exit status: 0 when the checkout is on the recorded commit, 1 otherwise.
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 <name> <version> <clone-directory>" >&2
  exit 2
fi

name="$1"
version="$2"
dir="$3"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
checksums="${here}/../dependency-checksums.txt"

actual="$(git -C "${dir}" rev-parse HEAD)"

expected="$(awk -v n="${name}" -v v="${version}" \
  '$0 !~ /^[[:space:]]*#/ && $1 == n && $2 == v { print $3; exit }' "${checksums}")"

if [ -z "${expected}" ]; then
  echo "::error::No commit recorded for ${name} ${version}." >&2
  echo "Add a line to .github/dependency-checksums.txt before bumping a dependency:" >&2
  echo "  ${name} ${version} ${actual}" >&2
  exit 1
fi

if [ "${expected}" = "unrecorded" ]; then
  echo "::warning::${name} ${version} is cloned without verifying which commit it lands on." >&2
  echo "::warning::Record it in .github/dependency-checksums.txt; this run saw ${actual}" >&2
  exit 0
fi

if [ "${actual}" != "${expected}" ]; then
  echo "::error::${name} ${version} is not on the recorded commit (${dir})" >&2
  echo "  expected: ${expected}" >&2
  echo "  actual  : ${actual}" >&2
  echo "The tag has moved. Review what changed before updating the recorded commit." >&2
  exit 1
fi

echo "${name} ${version}: commit ${actual} (verified)"
