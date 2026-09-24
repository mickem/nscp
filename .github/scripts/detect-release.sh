#!/usr/bin/env bash
#
# Decide whether a push to main is a release, and which version it releases.
#
# A release is cut by the release pull request, which renames
# docs/upgrades/next to docs/upgrades/<version> (see docs/upgrades/README.md).
# So a push is a release when it adds a directory docs/upgrades/<version>/ that
# did not exist before the push and is not a tag yet. A feature adds its note
# under docs/upgrades/next/, which is not a version; a late note for a release
# that is already out lands in a directory that already exists (and is tagged).
# Neither is a release.
#
# The directory name is the version. It is compared with the one git-version
# derives from the commit subjects, but only warned about: the release PR is
# where the number is chosen, and it has been chosen by hand before (0.18.0
# is a minor release with no feature commit in it).
#
# A manual run (workflow_dispatch) names the version itself. It is how a
# release is re-cut when its build failed for a reason that had nothing to do
# with the code; the version must not be a tag yet.
#
# Environment:
#   EVENT_NAME       push | workflow_dispatch
#   BEFORE_SHA       the push's "before" commit (push only; may be all zeros)
#   INPUT_VERSION    the version asked for (workflow_dispatch only)
#   DERIVED_VERSION  what git-version computed, for the cross-check (optional)
#   HEAD_SHA         commit to inspect, default HEAD (for running by hand)
#
# Writes release=true|false and version=<version> to $GITHUB_OUTPUT (stdout
# when unset). Runnable by hand in a full clone:
#
#   EVENT_NAME=push BEFORE_SHA=<sha> HEAD_SHA=<sha> .github/scripts/detect-release.sh
set -euo pipefail

out="${GITHUB_OUTPUT:-/dev/stdout}"
head="${HEAD_SHA:-HEAD}"

emit() {
  echo "release=$1" >> "$out"
  echo "version=$2" >> "$out"
}

is_version() {
  [[ "$1" =~ ^[0-9]+(\.[0-9]+)+$ ]]
}

is_tagged() {
  git rev-parse -q --verify "refs/tags/$1" > /dev/null
}

check_derived() {
  if [ -n "${DERIVED_VERSION:-}" ] && [ "$DERIVED_VERSION" != "$1" ]; then
    echo "::warning title=Release version::Releasing $1, but the commit subjects since the last tag would make it $DERIVED_VERSION. Building $1 as named."
  fi
}

case "${EVENT_NAME:-}" in
  workflow_dispatch)
    version="${INPUT_VERSION:-}"
    if ! is_version "$version"; then
      echo "::error title=Release version::'$version' is not a version number such as 0.24.0."
      exit 1
    fi
    if is_tagged "$version"; then
      echo "::error title=Release version::$version is already tagged, so it has been published. Cut a new version instead."
      exit 1
    fi
    check_derived "$version"
    echo "Manual release of $version from $(git rev-parse --short "$head")"
    emit true "$version"
    exit 0
    ;;
  push) ;;
  *)
    echo "::error::EVENT_NAME must be push or workflow_dispatch, not '${EVENT_NAME:-}'"
    exit 2
    ;;
esac

# What the push changed. "before" is the previous tip of main; it is all zeros
# for a new branch and not an ancestor after a force push, and then only the
# pushed commit itself is looked at.
base=""
before="${BEFORE_SHA:-}"
if [ -n "$before" ] && [ "$before" != "0000000000000000000000000000000000000000" ] \
    && git cat-file -e "${before}^{commit}" 2> /dev/null \
    && git merge-base --is-ancestor "$before" "$head"; then
  base="$before"
elif git rev-parse -q --verify "${head}^" > /dev/null; then
  base="${head}^"
else
  echo "No parent to compare with: not a release."
  emit false ""
  exit 0
fi

# --no-renames: the release PR renames every note, and a rename is not an "A".
versions=()
while IFS= read -r dir; do
  [ -n "$dir" ] || continue
  if ! is_version "$dir"; then
    continue
  fi
  if git cat-file -e "${base}:docs/upgrades/${dir}" 2> /dev/null; then
    echo "docs/upgrades/$dir/ already existed: a note for a published release, not a release."
    continue
  fi
  if is_tagged "$dir"; then
    echo "$dir is already tagged: not a release."
    continue
  fi
  versions+=("$dir")
done < <(git diff --no-renames --name-only --diff-filter=A "$base" "$head" -- docs/upgrades/ \
           | sed -n 's#^docs/upgrades/\([^/]*\)/.*#\1#p' | sort -u)

case "${#versions[@]}" in
  0)
    echo "No new docs/upgrades/<version>/ directory: not a release."
    emit false ""
    ;;
  1)
    check_derived "${versions[0]}"
    echo "Release ${versions[0]}: docs/upgrades/${versions[0]}/ was added by this push."
    emit true "${versions[0]}"
    ;;
  *)
    echo "::error title=Release version::This push adds several version directories (${versions[*]}) under docs/upgrades/. A release PR renames next to exactly one."
    exit 1
    ;;
esac
