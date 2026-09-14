#!/usr/bin/env bash
#
# Verify a downloaded build dependency against the digest recorded for it in
# .github/dependency-checksums.txt. See .github/actions/verify-checksum for why
# this exists; this script is the implementation and is also runnable by hand:
#
#   .github/scripts/verify-checksum.sh openssl 3.5.8 tmp/openssl-3.5.8.tar.gz
#
# Exit status: 0 when the digest matches (or the entry is deliberately marked
# unrecorded), 1 for a mismatch, a missing entry, or a missing file.
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 <name> <version> <file>" >&2
  exit 2
fi

name="$1"
version="$2"
file="$3"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
checksums="${here}/../dependency-checksums.txt"

if [ ! -f "${checksums}" ]; then
  echo "::error::${checksums} not found" >&2
  exit 1
fi
if [ ! -f "${file}" ]; then
  echo "::error::${file} not found; nothing to verify for ${name} ${version}" >&2
  exit 1
fi

# Fields: name version sha256 [comment]. Comments and blank lines are skipped.
expected="$(awk -v n="${name}" -v v="${version}" \
  '$0 !~ /^[[:space:]]*#/ && $1 == n && $2 == v { print $3; exit }' "${checksums}")"

if [ -z "${expected}" ]; then
  echo "::error::No checksum recorded for ${name} ${version}." >&2
  echo "Add a line to .github/dependency-checksums.txt before bumping a dependency:" >&2
  echo "  ${name} ${version} $(sha256sum "${file}" | cut -d' ' -f1)" >&2
  echo "Record it from a copy you have verified independently, not from this run:" >&2
  echo "the whole point is that the build cannot vouch for what it just downloaded." >&2
  exit 1
fi

if [ "${expected}" = "unrecorded" ]; then
  echo "::warning::${name} ${version} is downloaded without integrity verification." >&2
  echo "::warning::Record its digest in .github/dependency-checksums.txt; this run saw" \
       "$(sha256sum "${file}" | cut -d' ' -f1)" >&2
  exit 0
fi

actual="$(sha256sum "${file}" | cut -d' ' -f1)"
if [ "${actual}" != "${expected}" ]; then
  echo "::error::Checksum mismatch for ${name} ${version} (${file})" >&2
  echo "  expected: ${expected}" >&2
  echo "  actual  : ${actual}" >&2
  exit 1
fi

echo "${name} ${version}: sha256 ${actual} (verified)"
