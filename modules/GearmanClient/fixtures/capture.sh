#!/bin/sh
# Regenerate the Mod-Gearman payload fixtures in this directory from the
# test images: build tests/Dockerfiles/<core>-gearman.Dockerfile and run it
# once in capture mode (CAPTURE_DIR set), which makes the entrypoint dump one
# host job, one service job and three send_gearman results, then exit.
#
#   capture.sh            both cores
#   capture.sh nagios     just one (nagios | naemon)
#
# See README.md for what the files are and how to run the same capture
# natively without docker.
set -eu

here="$(cd "$(dirname "$0")" && pwd)"
tests="$(cd "$here/../../../tests" && pwd)"
cores="${*:-naemon nagios}"

for core in $cores; do
  case "$core" in
    nagios|naemon) ;;
    *) echo "unknown core '$core' (nagios|naemon)" >&2; exit 2 ;;
  esac
  image="nscp-it/$core-gearman"
  echo ">> Building $image from tests/Dockerfiles/$core-gearman.Dockerfile"
  docker build -t "$image" -f "$tests/Dockerfiles/$core-gearman.Dockerfile" "$tests"
  echo ">> Capturing $core payloads into $here"
  docker run --rm \
    -e CAPTURE_DIR=/capture \
    -e GEARMAN_KEY="$(cat "$here/key.txt")" \
    -v "$here:/capture" \
    "$image"
done

echo ">> Done. Now run: cd tests && npx jest --runInBand gearman-fixtures"
