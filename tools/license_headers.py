#!/usr/bin/env python3
"""
Ensure every C/C++ source/header file carries the NSClient++ GPL-2 header.

Modes
-----
  --check    Report files that need changes. Exits 1 if any do. (CI use.)
  --fix      Rewrite files in place to apply the canonical header.
  (default)  Dry-run summary, exits 0 regardless.

The script:
  * adds the canonical LGPL header to files that have no copyright line,
  * rewrites an existing NSClient++ / Michael Medin header to the canonical
    form (normalizing year, formatting, and license wording),
  * leaves files with a foreign copyright (third-party code) untouched.

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
THIRD_PARTY_PATH_FRAGMENTS = (
    "/libs/lua/",
    "/libs/minizip/",
    "/libs/mongoose-cpp/",
    "/libs/protobuf/",
    "/libs/protobuf_net/",
)

# Individual third-party files that don't trip the "Copyright" detector
# (e.g. they use "Rights reserved" or another wording). POSIX, repo-relative.
EXCLUDE_FILES = frozenset()

OWN_COPYRIGHT_PATTERNS = (
    re.compile(r"Michael\s+Medin", re.IGNORECASE),
    re.compile(r"NSClient\+\+\s+Authors", re.IGNORECASE),
)

CANONICAL_HEADER_TEMPLATE = """\
/*
 * Copyright (C) 2004-{year} Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */
"""


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

    block_bounds = find_first_block_comment(normalized)
    if block_bounds:
        start, end = block_bounds
        block = normalized[start:end]
        if header_block_is_ours(block):
            # Replace just the block with the canonical form.
            new = canonical + normalized[end:].lstrip("\n")
            # Preserve a single blank line between header and code.
            if not new.endswith("\n"):
                new += "\n"
            # Ensure exactly one blank line after the header.
            new = canonical + "\n" + normalized[end:].lstrip("\n")
            if new == normalized:
                return "ok", None, "canonical"
            return "updated", apply_eol(new, eol), "header normalized"
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
            new = normalized[:start] + canonical + normalized[end:].lstrip("\n")
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
