#!/bin/bash
# SPDX-FileCopyrightText: 2004-2026 Michael Medin
# SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#
# Make a staged macOS install tree self-contained.
#
# The build links against Homebrew's Boost, OpenSSL, protobuf and Lua, and the
# Mach-O images come out of the linker referring to them by absolute path
# (/opt/homebrew/opt/boost/lib/libboost_thread.dylib). That is fine on the build
# machine and useless in a package: install it on a Mac without Homebrew - which
# is every Mac a monitoring agent is actually deployed to - and the daemon dies
# at load time with "Library not loaded". Worse, on a Mac *with* Homebrew it
# starts, against whatever version of Boost that machine happens to have.
#
# So every non-system dylib the tree references is copied in next to our own
# private libraries and the references are rewritten to @rpath, which the
# RPATHs CMake already emits ($ORIGIN's Mach-O spelling, @loader_path) resolve.
#
# Usage: bundle_dylibs.sh <staging-root> <libdir-relative-to-root>
#   e.g. bundle_dylibs.sh stage usr/local/lib/nsclient
set -euo pipefail

STAGE="${1:?usage: bundle_dylibs.sh <staging-root> <libdir-relative-to-root>}"
LIBREL="${2:?usage: bundle_dylibs.sh <staging-root> <libdir-relative-to-root>}"
LIBDIR="$STAGE/$LIBREL"

[ -d "$LIBDIR" ] || { echo "No such directory: $LIBDIR" >&2; exit 1; }

# Every Mach-O in the tree, into the array IMAGES. -perm -u+x alone would miss
# the dylibs (they are 0644) and a plain extension match would miss the
# executables, so the candidates are filtered by asking `file` what they are -
# which also drops the shell scripts in sbin.
#
# A snapshot rather than a stream: the loops below copy new dylibs into the very
# directory this walks, and re-enumerating while that happens is how you get a
# traversal that sees a file half-written.
IMAGES=()
collect_macho() {
    IMAGES=()
    local f
    while IFS= read -r -d '' f; do
        if /usr/bin/file -b "$f" | /usr/bin/grep -q 'Mach-O'; then
            IMAGES+=("$f")
        fi
    done < <(/usr/bin/find "$STAGE" -type f \
        \( -name '*.dylib' -o -name '*.so' -o -perm -u+x \) -print0)
}

# The dependencies of one image, minus the ones that are not ours to carry:
#
#   /usr/lib/*, /System/*   part of macOS. Bundling these is not merely
#                           unnecessary, it is forbidden - the dyld shared cache
#                           is the only copy, and a duplicate libSystem would be
#                           rejected by the loader.
#   @rpath, @loader_path,
#   @executable_path        already relocated (ours, or rewritten by an earlier
#                           pass of this script).
#
# The first line of otool -L output is the image's own install name, not a
# dependency, hence the tail.
deps_of() {
    /usr/bin/otool -L "$1" | /usr/bin/tail -n +2 | /usr/bin/awk '{print $1}' |
        /usr/bin/grep -v -E '^(/usr/lib/|/System/|@rpath/|@loader_path/|@executable_path/)' || true
}

echo "Bundling external dylibs into $LIBREL"

# Copying a dylib in brings that dylib's own dependencies into the tree, which
# then have to be bundled too, so this iterates to a fixpoint rather than making
# a single pass. The bound is a safety net, not an expected depth: Boost on
# OpenSSL on nothing is two.
pass=0
while :; do
    pass=$((pass + 1))
    if [ "$pass" -gt 10 ]; then
        echo "::error::dylib bundling did not converge after 10 passes" >&2
        exit 1
    fi
    copied=0

    collect_macho
    for image in "${IMAGES[@]}"; do
        # A dylib copied in from Homebrew is read-only; install_name_tool needs
        # to write to it.
        /bin/chmod u+w "$image"

        for dep in $(deps_of "$image"); do
            base="$(/usr/bin/basename "$dep")"
            if [ ! -f "$LIBDIR/$base" ]; then
                if [ ! -f "$dep" ]; then
                    echo "::error::$image needs $dep, which does not exist on this machine" >&2
                    exit 1
                fi
                echo "  + $base  (from $dep)"
                # -L: Homebrew's lib/libfoo.dylib is usually a symlink into the
                # versioned name. Copy the file, not the link.
                /bin/cp -L "$dep" "$LIBDIR/$base"
                /bin/chmod u+w "$LIBDIR/$base"
                # An install name of @rpath/<base> is what lets a consumer find
                # it through the RPATH it already carries, wherever the prefix
                # ends up.
                /usr/bin/install_name_tool -id "@rpath/$base" "$LIBDIR/$base" 2>/dev/null
                copied=$((copied + 1))
            fi
            /usr/bin/install_name_tool -change "$dep" "@rpath/$base" "$image" 2>/dev/null
        done
    done

    # `[ ... ] && break` would be a failing AND-OR list as the last command of
    # the loop body on every pass that copied something, which under `set -e`
    # aborts the script.
    if [ "$copied" -eq 0 ]; then
        break
    fi
done

# The bundled third-party dylibs sit in the same directory as each other, so
# they resolve their siblings through @loader_path - the same RPATH our own
# private libraries carry (NSCP_RPATH_LIB). CMake sets that on the targets it
# builds; these were copied in behind its back, so set it here.
#
# install_name_tool fails when the RPATH is already present, which is the normal
# case on a re-run, so the failure is ignored rather than guarded against.
collect_macho
for image in "${IMAGES[@]}"; do
    case "$image" in
        "$LIBDIR"/*.dylib)
            /usr/bin/install_name_tool -add_rpath "@loader_path" "$image" 2>/dev/null || true
            ;;
    esac
done

# Re-sign. This is not optional on Apple silicon: every arm64 Mach-O must carry
# a valid signature to be executed or loaded at all, and install_name_tool
# invalidates the one it was built with. An ad-hoc signature ("-") satisfies the
# loader; a Developer ID signature, when the workflow has credentials for one,
# is applied later and replaces it.
echo "Re-signing (ad-hoc) after install-name rewriting"
for image in "${IMAGES[@]}"; do
    /usr/bin/codesign --force --sign - --timestamp=none "$image" >/dev/null 2>&1 ||
        echo "::warning::could not sign $image"
done

# A reference the loader cannot satisfy is worth catching here, where the error
# names the file, rather than on a customer's machine at three in the morning.
echo "Verifying no absolute non-system references remain"
remaining=0
for image in "${IMAGES[@]}"; do
    for dep in $(deps_of "$image"); do
        echo "::error::$image still references $dep"
        remaining=$((remaining + 1))
    done
done
[ "$remaining" -eq 0 ] || exit 1

echo "Bundling complete."
