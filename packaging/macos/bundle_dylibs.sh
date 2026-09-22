#!/bin/bash
# SPDX-FileCopyrightText: 2004-2026 Michael Medin
# SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#
# Make a staged macOS install tree self-contained.
#
# The build links against Homebrew's Boost, OpenSSL, protobuf and Lua, and the
# Mach-O images come out of the linker referring to them by absolute path
# (/opt/homebrew/opt/protobuf/lib/libprotobuf.36.2.0.dylib). That is fine on the
# build machine and useless in a package: install it on a Mac without Homebrew -
# which is every Mac a monitoring agent is actually deployed to - and the daemon
# dies at load time with "Library not loaded". Worse, on a Mac *with* Homebrew it
# starts, against whatever version of Boost that machine happens to have.
#
# So every non-system dylib the tree references is copied in next to our own
# private libraries and the references are rewritten to @rpath, which the
# RPATHs CMake already emits ($ORIGIN's Mach-O spelling, @loader_path) resolve.
#
# Two kinds of reference have to be followed, and missing the second is what
# shipped a broken package once already:
#
#   absolute   /opt/homebrew/...         copy it in, rewrite the reference.
#   @rpath/X   inside a library we just
#              copied in                 Homebrew rewrites a formula's *id* to an
#                                        absolute path but leaves its references
#                                        to its own siblings as @rpath, resolved
#                                        through that library's LC_RPATH. So
#                                        libprotobuf came in, its @rpath
#                                        reference to libutf8_validity was read
#                                        as "already relocated", and the daemon
#                                        failed at launch:
#                                          Library not loaded:
#                                            @rpath/libutf8_validity.36.2.0.dylib
#                                          Referenced from: .../libprotobuf...dylib
#                                        Those are resolved against the *original*
#                                        library's rpaths and copied in too.
#
# Usage: bundle_dylibs.sh <staging-root> <libdir-relative-to-root>
#   e.g. bundle_dylibs.sh stage usr/local/lib/nsclient
set -euo pipefail

STAGE="${1:?usage: bundle_dylibs.sh <staging-root> <libdir-relative-to-root>}"
LIBREL="${2:?usage: bundle_dylibs.sh <staging-root> <libdir-relative-to-root>}"
LIBDIR="$STAGE/$LIBREL"

[ -d "$LIBDIR" ] || { echo "No such directory: $LIBDIR" >&2; exit 1; }

# The Mach-O tools, by absolute path so a stray PATH cannot redirect them, but
# overridable so the logic can be exercised against stubs off a Mac.
: "${OTOOL:=/usr/bin/otool}"
: "${INSTALL_NAME_TOOL:=/usr/bin/install_name_tool}"
: "${CODESIGN:=/usr/bin/codesign}"
: "${FILE_CMD:=/usr/bin/file}"

# Where each bundled library came from, so its @rpath references can be resolved
# the way the linker would have. A directory of files rather than an associative
# array: /bin/bash on macOS is 3.2, which has no `declare -A`.
ORIGINS="$(mktemp -d)"
trap 'rm -rf "$ORIGINS"' EXIT
record_origin() { printf '%s\n' "$2" > "$ORIGINS/$1"; }
origin_of() { [ -f "$ORIGINS/$1" ] && cat "$ORIGINS/$1"; }

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
        if "$FILE_CMD" -b "$f" | /usr/bin/grep -q 'Mach-O'; then
            IMAGES+=("$f")
        fi
    done < <(/usr/bin/find "$STAGE" -type f \
        \( -name '*.dylib' -o -name '*.so' -o -perm -u+x \) -print0)
}

# The image's dependencies, by kind. The first line of otool -L is the image's
# own install name, not a dependency, hence the tail.
#
# /usr/lib/* and /System/* are part of macOS. Bundling those is not merely
# unnecessary, it is forbidden - the dyld shared cache is the only copy.
deps_absolute() {
    "$OTOOL" -L "$1" | /usr/bin/tail -n +2 | /usr/bin/awk '{print $1}' |
        /usr/bin/grep -v -E '^(/usr/lib/|/System/|@)' || true
}
deps_rpath() {
    "$OTOOL" -L "$1" | /usr/bin/tail -n +2 | /usr/bin/awk '{print $1}' |
        /usr/bin/grep -E '^@rpath/' | /usr/bin/sed 's|^@rpath/||' || true
}

# The LC_RPATH entries recorded in a Mach-O.
lc_rpaths() {
    "$OTOOL" -l "$1" |
        /usr/bin/awk '/ cmd LC_RPATH/{f=1} f && / path /{print $2; f=0}'
}

# Where the original of a bundled library would have found its own @rpath/$2.
# Searches the source library's recorded rpaths (with @loader_path expanded to
# its directory), then the directory it came from.
resolve_rpath_dep() {
    local image="$1" base="$2"
    local origin odir p
    origin="$(origin_of "$(/usr/bin/basename "$image")")" || return 1
    [ -n "$origin" ] || return 1
    odir="$(/usr/bin/dirname "$origin")"
    for p in $(lc_rpaths "$origin"); do
        case "$p" in
            @loader_path*) p="${odir}${p#@loader_path}" ;;
            @*) continue ;;
        esac
        if [ -f "$p/$base" ]; then printf '%s\n' "$p/$base"; return 0; fi
    done
    [ -f "$odir/$base" ] && { printf '%s\n' "$odir/$base"; return 0; }
    return 1
}

# Copy one external library in and remember where it came from.
bundle_one() {
    local src="$1" base="$2"
    echo "  + $base  (from $src)"
    # -L: Homebrew's lib/libfoo.dylib is usually a symlink into the versioned
    # name. Copy the file, not the link.
    /bin/cp -L "$src" "$LIBDIR/$base"
    /bin/chmod u+w "$LIBDIR/$base"
    # An install name of @rpath/<base> is what lets a consumer find it through
    # the RPATH it already carries, wherever the prefix ends up.
    "$INSTALL_NAME_TOOL" -id "@rpath/$base" "$LIBDIR/$base" 2>/dev/null
    record_origin "$base" "$src"
}

echo "Bundling external dylibs into $LIBREL"

# Copying a dylib in brings that dylib's own dependencies into the tree, which
# then have to be bundled too, so this iterates to a fixpoint rather than making
# a single pass. The bound is a safety net, not an expected depth.
pass=0
while :; do
    pass=$((pass + 1))
    if [ "$pass" -gt 10 ]; then
        echo "::error::dylib bundling did not converge after 10 passes" >&2
        exit 1
    fi
    copied=0

    collect_macho
    for image in ${IMAGES[@]+"${IMAGES[@]}"}; do
        # A dylib copied in from Homebrew is read-only; install_name_tool needs
        # to write to it.
        /bin/chmod u+w "$image"

        for dep in $(deps_absolute "$image"); do
            base="$(/usr/bin/basename "$dep")"
            if [ ! -f "$LIBDIR/$base" ]; then
                if [ ! -f "$dep" ]; then
                    echo "::error::$image needs $dep, which does not exist on this machine" >&2
                    exit 1
                fi
                bundle_one "$dep" "$base"
                copied=$((copied + 1))
            fi
            "$INSTALL_NAME_TOOL" -change "$dep" "@rpath/$base" "$image" 2>/dev/null
        done

        # @rpath references. Ours already sit in LIBDIR; a bundled Homebrew
        # library's point at its own siblings and have to be followed.
        for base in $(deps_rpath "$image"); do
            [ -f "$LIBDIR/$base" ] && continue
            if src="$(resolve_rpath_dep "$image" "$base")"; then
                bundle_one "$src" "$base"
                copied=$((copied + 1))
            fi
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
# Their original absolute rpaths go at the same time. Those point into the build
# machine's Homebrew prefix: harmless on a Mac without Homebrew, but on a Mac
# *with* one they are searched first, and the bundle would silently load that
# machine's copy of a library instead of the one it ships.
collect_macho
for image in ${IMAGES[@]+"${IMAGES[@]}"}; do
    case "$image" in
        "$LIBDIR"/*.dylib)
            # Fails when the RPATH is already present, which is the normal case
            # on a re-run, so the failure is ignored rather than guarded against.
            "$INSTALL_NAME_TOOL" -add_rpath "@loader_path" "$image" 2>/dev/null || true
            ;;
    esac
    for p in $(lc_rpaths "$image"); do
        case "$p" in
            /*) "$INSTALL_NAME_TOOL" -delete_rpath "$p" "$image" 2>/dev/null || true ;;
        esac
    done
done

# Re-sign. This is not optional on Apple silicon: every arm64 Mach-O must carry
# a valid signature to be executed or loaded at all, and install_name_tool
# invalidates the one it was built with. An ad-hoc signature ("-") satisfies the
# loader; a Developer ID signature, when the workflow has credentials for one,
# is applied later and replaces it.
echo "Re-signing (ad-hoc) after install-name rewriting"
for image in ${IMAGES[@]+"${IMAGES[@]}"}; do
    "$CODESIGN" --force --sign - --timestamp=none "$image" >/dev/null 2>&1 ||
        echo "::warning::could not sign $image"
done

# Every reference the loader will make must be satisfiable from inside the tree.
# Checking only the absolute ones is what let the libutf8_validity failure
# through: it was an @rpath reference that resolved to nothing, and nothing here
# looked at those.
echo "Verifying every reference resolves inside the bundle"
remaining=0
for image in ${IMAGES[@]+"${IMAGES[@]}"}; do
    for dep in $(deps_absolute "$image"); do
        echo "::error::$image still references $dep"
        remaining=$((remaining + 1))
    done
    for base in $(deps_rpath "$image"); do
        if [ ! -f "$LIBDIR/$base" ]; then
            echo "::error::$image needs @rpath/$base, which is not in $LIBREL"
            remaining=$((remaining + 1))
        fi
    done
done
[ "$remaining" -eq 0 ] || exit 1

echo "Bundled $(/usr/bin/find "$LIBDIR" -maxdepth 1 -name '*.dylib' | /usr/bin/wc -l | tr -d ' ') libraries in $LIBREL; all references resolve."
