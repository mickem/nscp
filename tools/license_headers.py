#!/usr/bin/env python3
"""
Ensure every C/C++ source/header file carries the canonical NSClient++ SPDX
license header:

    // SPDX-FileCopyrightText: 2004-<year> Michael Medin
    // SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

Modes
-----
  --check    Report files that need changes. Exits 1 if any do. (CI use.)
  --fix      Rewrite files in place to apply the canonical header.
  (default)  Dry-run summary, exits 0 regardless.

The script:
  * adds the canonical SPDX header to files that have no copyright line,
  * rewrites an existing NSClient++ / Michael Medin header (whether the old
    GPL block comment or an earlier SPDX line) to the canonical form,
    normalizing the year,
  * applies a combined MIT header (upstream + NSClient++ copyright) to the
    handful of mongoose-cpp files derived from Gregwar/mongoose-cpp, so the
    required upstream MIT attribution is preserved per file (DERIVED_MIT_FILES),
  * leaves files with a foreign copyright (third-party code) untouched,
  * leaves explicitly listed third-party files untouched even when their
    copyright sits below the scan window (EXCLUDE_FILES).

Run from anywhere; the repo root is auto-detected from the script's path,
or passed via --root.
"""

import argparse
import datetime
import os
import re
import sys
from pathlib import Path

SOURCE_EXTS = {".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx", ".hh"}

# Directories never to descend into.
EXCLUDE_DIRS = {
    ".git", "build", "cmake-build", "cmake-build-debug",
    "cmake-build-debug-wsl", "cmake-build-debug-wsl-coverage",
    "ext", "node_modules", "third_party", "vendor",
}

# Path fragments (POSIX style) that mark third-party code we never modify,
# even when the file has no copyright at all.
#
# Note: libs/mongoose-cpp/ is intentionally NOT here. It is a vendored copy of
# Gregwar/mongoose-cpp that has since been substantially rewritten: the files
# still derived from the upstream MIT code are handled via DERIVED_MIT_FILES
# (combined MIT attribution), while the genuinely new files get the canonical
# NSClient++ header through the normal flow.
THIRD_PARTY_PATH_FRAGMENTS = (
    "/libs/lua/",
    "/libs/minizip/",
    "/libs/protobuf/",
    "/libs/protobuf_net/",
)

# Individual third-party files we never modify. These either carry their
# upstream license below the scan window (so auto-detection would miss it) or
# we simply want their exclusion documented explicitly. POSIX, repo-relative.
EXCLUDE_FILES = frozenset({
    "include/net/icmp_header.hpp",   # Boost Software License 1.0 (Christopher M. Kohlhoff)
    "include/net/ipv4_header.hpp",   # Boost Software License 1.0 (Christopher M. Kohlhoff)
    "include/simpleini/simpleini.h", # MIT (Brodie Thiesfield); copyright sits below the scan window
})

OWN_COPYRIGHT_PATTERNS = (
    re.compile(r"Michael\s+Medin", re.IGNORECASE),
    re.compile(r"NSClient\+\+\s+Authors", re.IGNORECASE),
)

CANONICAL_HEADER_TEMPLATE = """\
// SPDX-FileCopyrightText: 2004-{year} Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
"""

# Header for the mongoose-cpp files derived from Gregwar/mongoose-cpp. They were
# originally MIT-licensed; MIT requires the upstream copyright notice be kept, so
# we preserve Passault's line alongside the NSClient++ copyright and keep the
# file under MIT (the upstream license) rather than the project's dual license.
MIT_DERIVED_HEADER_TEMPLATE = """\
// SPDX-FileCopyrightText: 2013 Grégoire Passault
// SPDX-FileCopyrightText: 2004-{year} Michael Medin
// SPDX-License-Identifier: MIT
"""

# Files in libs/mongoose-cpp/ still derived from the upstream MIT code. Repo-
# relative POSIX paths. Everything else in that directory is original NSClient++
# work and gets the canonical header through the normal flow.
DERIVED_MIT_FILES = frozenset({
    "libs/mongoose-cpp/Request.h",
    "libs/mongoose-cpp/Request.cpp",
    "libs/mongoose-cpp/Response.h",
    "libs/mongoose-cpp/Response.cpp",
    "libs/mongoose-cpp/Controller.h",
    "libs/mongoose-cpp/Controller.cpp",
    "libs/mongoose-cpp/StreamResponse.h",
    "libs/mongoose-cpp/StreamResponse.cpp",
    "libs/mongoose-cpp/Server.h",
    "libs/mongoose-cpp/RequestHandler.h",
})


def find_repo_root(start: Path) -> Path:
    """Walk up from start until we find a directory that looks like the repo."""
    for parent in [start] + list(start.parents):
        if (parent / ".git").exists() and (parent / "CMakeLists.txt").exists():
            return parent
    return start


def iter_source_files(root: Path, paths):
    """Yield source files under `root`, honoring `paths` if non-empty."""
    roots = [Path(p) if Path(p).is_absolute() else (root / p) for p in paths] or [root]
    for r in roots:
        if r.is_file():
            if r.suffix.lower() in SOURCE_EXTS:
                yield r
            continue
        for dirpath, dirnames, filenames in os.walk(r):
            dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
            for name in filenames:
                if Path(name).suffix.lower() in SOURCE_EXTS:
                    yield Path(dirpath) / name


def is_in_third_party_path(path: Path, root: Path) -> bool:
    posix = "/" + str(path.relative_to(root)).replace(os.sep, "/") + "/"
    return any(frag in posix for frag in THIRD_PARTY_PATH_FRAGMENTS)


def is_excluded_file(path: Path, root: Path) -> bool:
    rel = str(path.relative_to(root)).replace(os.sep, "/")
    return rel in EXCLUDE_FILES


def find_first_block_comment(text: str):
    """
    Return (start, end_exclusive) of the first /* ... */ block if it appears
    before any non-whitespace, non-BOM, non-#! content. Otherwise return None.
    """
    i = 0
    n = len(text)
    if n >= 1 and text[0] == "﻿":
        i = 1
    # Skip a leading shebang (defensive — unusual for C++).
    if text.startswith("#!", i):
        nl = text.find("\n", i)
        if nl == -1:
            return None
        i = nl + 1
    # Skip whitespace only.
    while i < n and text[i] in " \t\r\n":
        i += 1
    if not text.startswith("/*", i):
        return None
    start = i
    end = text.find("*/", i + 2)
    if end == -1:
        return None
    return start, end + 2


def find_header_start(text: str) -> int:
    """Index of the first 'real' content, skipping a BOM, a shebang line, and
    any leading whitespace. This is where a header is expected to begin."""
    i = 0
    n = len(text)
    if n >= 1 and text[0] == "﻿":
        i = 1
    if text.startswith("#!", i):
        nl = text.find("\n", i)
        i = n if nl == -1 else nl + 1
    while i < n and text[i] in " \t\r\n":
        i += 1
    return i


def extract_leading_spdx(text: str, start: int):
    """If the run of consecutive `//` line comments beginning at `start` forms
    an SPDX header (contains an "SPDX-" tag), return (start, end_exclusive) of
    that run. Otherwise return None.
    """
    if not text.startswith("//", start):
        return None
    n = len(text)
    i = start
    end = start
    saw_spdx = False
    while i < n and text.startswith("//", i):
        nl = text.find("\n", i)
        line_end = n if nl == -1 else nl
        if "SPDX-" in text[i:line_end]:
            saw_spdx = True
        if nl == -1:
            end = n
            break
        end = nl + 1
        i = nl + 1
    if not saw_spdx:
        return None
    return start, end


def header_block_is_ours(block: str) -> bool:
    if "Copyright" not in block:
        return False
    return any(p.search(block) for p in OWN_COPYRIGHT_PATTERNS)


def block_has_foreign_copyright(block: str) -> bool:
    return "Copyright" in block and not any(p.search(block) for p in OWN_COPYRIGHT_PATTERNS)


def detect_eol(text: str) -> str:
    """Return the dominant line ending in the file (defaults to \\n)."""
    if "\r\n" in text and text.count("\r\n") * 2 >= text.count("\n"):
        return "\r\n"
    return "\n"


def apply_eol(text: str, eol: str) -> str:
    if eol == "\n":
        return text
    # Normalize then re-emit.
    return text.replace("\r\n", "\n").replace("\n", eol)


def build_canonical_header(year: int) -> str:
    return CANONICAL_HEADER_TEMPLATE.format(year=year)


def has_any_copyright_near_top(text: str, scan_chars: int = 4096) -> bool:
    return "Copyright" in text[:scan_chars]


def first_copyright_block_or_line(text: str, scan_chars: int = 4096) -> str:
    return text[:scan_chars]


def _find_block_containing_our_copyright(text: str, limit: int):
    """Locate the /* ... */ block within text[:limit] that contains an
    own-copyright marker. Returns (start, end_exclusive) or None.
    """
    i = 0
    while True:
        start = text.find("/*", i)
        if start == -1 or start >= limit:
            return None
        end = text.find("*/", start + 2)
        if end == -1:
            return None
        end += 2
        block = text[start:end]
        if any(p.search(block) for p in OWN_COPYRIGHT_PATTERNS):
            return start, end
        i = end


def replace_or_prepend_header(normalized: str, header: str) -> str:
    """Put `header` at the top of `normalized` (\\n EOL), replacing a leading
    SPDX line-comment run or a leading own /* ... */ block if present, else
    prepending. Used for files whose header is fully managed by this script
    (the canonical and the MIT-derived headers)."""
    hstart = find_header_start(normalized)
    spdx_bounds = extract_leading_spdx(normalized, hstart)
    if spdx_bounds:
        start, end = spdx_bounds
        return normalized[:start] + header + "\n" + normalized[end:].lstrip("\n")
    block_bounds = find_first_block_comment(normalized)
    if block_bounds:
        start, end = block_bounds
        if header_block_is_ours(normalized[start:end]):
            return normalized[:start] + header + "\n" + normalized[end:].lstrip("\n")
    i = 0
    while i < len(normalized) and normalized[i] in "\r\n":
        i += 1
    return header + "\n" + normalized[i:]


def classify_and_transform(path: Path, text: str, year: int, root: Path):
    """
    Decide what to do with a file. Returns (action, new_text_or_none, reason).

    action is one of:
      "ok"            : already canonical, no change.
      "updated"       : rewritten to canonical form.
      "added"         : canonical header prepended.
      "skipped-3p"    : foreign copyright, untouched.
      "skipped-path"  : in known third-party path with no header.
    """
    if is_excluded_file(path, root):
        return "skipped-path", None, "explicit exclude list"

    eol = detect_eol(text)
    normalized = text.replace("\r\n", "\n")
    canonical = build_canonical_header(year)

    # mongoose-cpp files derived from upstream MIT code: apply the combined MIT
    # header (upstream + NSClient++ copyright). Handled before the generic
    # detection below so a re-run does not mistake the Michael Medin line for a
    # cue to rewrite it to the canonical Apache/GPL header.
    rel = str(path.relative_to(root)).replace(os.sep, "/")
    if rel in DERIVED_MIT_FILES:
        header = MIT_DERIVED_HEADER_TEMPLATE.format(year=year)
        new = replace_or_prepend_header(normalized, header)
        if new == normalized:
            return "ok", None, "canonical (MIT derived)"
        return "updated", apply_eol(new, eol), "MIT derived header applied"

    # Case 1: a leading SPDX line-comment header already exists. If it's ours,
    # normalize it (e.g. bump the year); if it's a third party's SPDX header,
    # leave it. Checked first so a re-run on an already-converted file reports
    # "ok" rather than treating the "Copyright" in SPDX-FileCopyrightText as a
    # foreign copyright further down.
    hstart = find_header_start(normalized)
    spdx_bounds = extract_leading_spdx(normalized, hstart)
    if spdx_bounds:
        start, end = spdx_bounds
        block = normalized[start:end]
        if any(p.search(block) for p in OWN_COPYRIGHT_PATTERNS):
            new = normalized[:start] + canonical + "\n" + normalized[end:].lstrip("\n")
            if new == normalized:
                return "ok", None, "canonical"
            return "updated", apply_eol(new, eol), "SPDX header normalized"
        return "skipped-3p", None, "foreign SPDX header"

    # Case 2: a leading /* ... */ block comment. The old canonical header was
    # such a block; ours gets replaced with the SPDX lines.
    block_bounds = find_first_block_comment(normalized)
    if block_bounds:
        start, end = block_bounds
        block = normalized[start:end]
        if header_block_is_ours(block):
            new = normalized[:start] + canonical + "\n" + normalized[end:].lstrip("\n")
            if new == normalized:
                return "ok", None, "canonical"
            return "updated", apply_eol(new, eol), "block header replaced with SPDX"
        if block_has_foreign_copyright(block):
            return "skipped-3p", None, "foreign copyright in block"
        # Block comment that isn't a copyright (e.g. a file-level doc comment).
        # Fall through to no-block treatment unless any Copyright sits nearby.

    if has_any_copyright_near_top(normalized):
        head = first_copyright_block_or_line(normalized)
        if any(p.search(head) for p in OWN_COPYRIGHT_PATTERNS):
            # Our copyright lives in a non-leading block. Find the block that
            # contains it and replace just that block — leave anything above
            # alone (it may be intentional).
            bounds = _find_block_containing_our_copyright(normalized, len(head))
            if bounds is None:
                return "skipped-3p", None, "own copyright in non-block form (manual review)"
            start, end = bounds
            new = normalized[:start] + canonical + "\n" + normalized[end:].lstrip("\n")
            if new == normalized:
                return "ok", None, "canonical (non-leading block)"
            return "updated", apply_eol(new, eol), "header normalized (non-leading block)"
        return "skipped-3p", None, "foreign copyright near top"

    if is_in_third_party_path(path, root):
        return "skipped-path", None, "third-party directory"

    # No copyright anywhere near the top: prepend canonical header.
    leading_ws = ""
    i = 0
    while i < len(normalized) and normalized[i] in "\r\n":
        leading_ws += normalized[i]
        i += 1
    body = normalized[i:]
    new = canonical + "\n" + body
    return "added", apply_eol(new, eol), "header added"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="exit non-zero if any file needs changes")
    mode.add_argument("--fix", action="store_true", help="apply changes in place")
    parser.add_argument("--year", type=int, default=datetime.datetime.now().year,
                        help="copyright end year (default: current year)")
    parser.add_argument("--root", type=Path, default=None,
                        help="repository root (default: auto-detect from script location)")
    parser.add_argument("-v", "--verbose", action="store_true", help="print per-file actions")
    parser.add_argument("paths", nargs="*", help="files or directories to scan (default: whole repo)")
    args = parser.parse_args(argv)

    script_path = Path(__file__).resolve()
    root = args.root.resolve() if args.root else find_repo_root(script_path.parent)

    counts = {"ok": 0, "updated": 0, "added": 0, "skipped-3p": 0, "skipped-path": 0, "error": 0}
    changed_files = []

    for path in iter_source_files(root, args.paths):
        try:
            raw = path.read_bytes()
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            try:
                text = raw.decode("latin-1")
            except Exception as e:
                counts["error"] += 1
                print(f"ERROR: {path}: {e}", file=sys.stderr)
                continue
        except OSError as e:
            counts["error"] += 1
            print(f"ERROR: {path}: {e}", file=sys.stderr)
            continue

        action, new_text, reason = classify_and_transform(path, text, args.year, root)
        counts[action] += 1
        rel = path.relative_to(root) if path.is_absolute() else path

        if action in ("updated", "added"):
            changed_files.append(rel)
            if args.fix:
                # Re-encode using the file's original encoding hint.
                try:
                    path.write_text(new_text, encoding="utf-8", newline="")
                except OSError as e:
                    print(f"ERROR writing {path}: {e}", file=sys.stderr)
                    counts["error"] += 1
                if args.verbose:
                    print(f"{action.upper():>8}  {rel}  ({reason})")
            else:
                if args.verbose or args.check:
                    print(f"{action.upper():>8}  {rel}  ({reason})")
        elif args.verbose:
            print(f"{action.upper():>8}  {rel}  ({reason})")

    print()
    print(f"Scanned root        : {root}")
    print(f"Already canonical   : {counts['ok']}")
    print(f"Header updated      : {counts['updated']}")
    print(f"Header added        : {counts['added']}")
    print(f"Skipped (3rd-party) : {counts['skipped-3p']}")
    print(f"Skipped (3p path)   : {counts['skipped-path']}")
    if counts["error"]:
        print(f"Errors              : {counts['error']}")

    needs_changes = counts["updated"] + counts["added"]
    if args.fix:
        print(f"\nApplied changes to {needs_changes} file(s).")
    elif needs_changes:
        print(f"\n{needs_changes} file(s) need changes. Re-run with --fix to apply.")
        if args.check:
            return 1
    else:
        print("\nAll source files have a valid header.")

    return 1 if counts["error"] else 0


if __name__ == "__main__":
    sys.exit(main())
