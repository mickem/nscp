#!/usr/bin/env bash
#
# Print the SHA-256 or commit recorded for a dependency in
# .github/dependency-checksums.txt, for use in a cache key:
#
#   .github/scripts/dependency-digest.sh lua 5.4.8
#
# A build cached under a key that carries the recorded value can only ever be
# restored for exactly that input. Re-recording a digest, or recording one for
# a dependency that used to be `unrecorded`, then invalidates the cache
# instead of restoring a build made from bytes nobody verified.
#
# Exit status: 0 with the value on stdout, 1 when there is no line for that
# name and version (the same condition fails verify-checksum.sh).
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <name> <version>" >&2
  exit 2
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
checksums="${here}/../dependency-checksums.txt"

value="$(awk -v n="$1" -v v="$2" \
  '$0 !~ /^[[:space:]]*#/ && $1 == n && $2 == v { print $3; exit }' "${checksums}")"

if [ -z "${value}" ]; then
  echo "::error::No checksum recorded for $1 $2 in .github/dependency-checksums.txt" >&2
  exit 1
fi
echo "${value}"
