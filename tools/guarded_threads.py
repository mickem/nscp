#!/usr/bin/env python3
"""
Ensure no background thread is started without an exception guard.

Why
---
A thread body that lets an exception escape calls std::terminate(), and the
whole agent goes down: every check on the host stops because one collector hit
one bad sample. Unlike the DLL entry points in nscapi_plugin_wrapper.hpp, a
thread body has no wrapper between it and the runtime, so the guard has to be
written by hand - and historically several threads simply did not have one
(the CheckDisk collector, the CheckSystem auxiliary collector, the socket
server pool, ...).

threads/guarded_thread.hpp turns that into something a reviewer cannot miss:
start a thread with threads::start_guarded_thread(), or wrap its body in
threads::run_guarded() / threads::run_io_context_guarded(). This script is the
enforcement, in the spirit of the "a filter_obj owns every value it exposes"
sweep in CLAUDE.md: it flags every raw thread construction that has no guard
near it.

Modes
-----
  --check    Report unguarded thread creations. Exits 1 if any. (CI use.)
  (default)  Same report, exits 0.

A site that genuinely cannot use the helper goes in ALLOWED with a comment
saying why - never silently.
"""

import argparse
import re
import sys
from pathlib import Path

SOURCE_EXTS = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hxx"}

# Directories the sweep never descends into: build output and vendored code.
EXCLUDE_DIRS = {
    ".git", "build", "node_modules", "third_party", "vendor",
    "libs/lua", "libs/minizip", "libs/protobuf", "libs/replxx",
}

# Trees scanned. Everything else (tests, samples, tools) is out of scope: a
# test that crashes fails the test, which is the point of a test.
SCAN_ROOTS = ("modules", "service", "libs", "include", "clients")

# Repo-relative POSIX paths exempt from the rule, each with its reason.
ALLOWED = {
    # The guard itself, and the test that proves it guards.
    "include/threads/guarded_thread.hpp",
    "include/threads/guarded_io_context.hpp",
    "include/threads/guarded_thread_test.cpp",
    # scoped_thread_group routes every body through threads::run_guarded; the
    # construction it does is the guarded one.
    "include/threads/scoped_thread_group.hpp",
}

# What a thread construction looks like. std::thread is included even though
# the codebase uses boost::thread: a new one should not slip in unguarded
# either.
CREATION = re.compile(
    r"new\s+boost::thread\b"
    r"|make_shared\s*<\s*boost::thread\s*>"
    r"|=\s*boost::thread\s*\(\s*[^)\s]"      # assignment from a thread with a body
    r"|\bboost::thread\s+\w+\s*\(\s*[^)\s]"  # named thread with a body
    r"|\bstd::thread\b"
    r"|\.create_thread\s*\("
)

# A guard counts if it appears within this many lines of the construction -
# far enough for a multi-line lambda argument, near enough to be the guard for
# this thread and not one further down the file.
GUARD_WINDOW = 12
GUARDS = ("start_guarded_thread", "run_guarded", "run_io_context_guarded")

# scoped_thread_group::create_thread() guards the body itself, so a file that
# uses one needs no guard at the call. The receiver's type cannot be worked
# out from the text, so the test is whether the file uses that class at all -
# a file mixing a scoped_thread_group with a bare boost::thread_group would
# slip through, which is the one hole in this sweep and has no instance today.
GROUP_GUARD = "scoped_thread_group"
GROUP_CREATION = re.compile(r"\.create_thread\s*\(")

# Test files carry their own rules (see SCAN_ROOTS), but a _test.cpp sitting
# inside a scanned tree still gets skipped.
TEST_SUFFIX = "_test.cpp"


def is_excluded(path: Path, root: Path) -> bool:
    rel = path.relative_to(root).as_posix()
    if rel in ALLOWED:
        return True
    if path.name.endswith(TEST_SUFFIX):
        return True
    for fragment in EXCLUDE_DIRS:
        if rel == fragment or rel.startswith(fragment + "/") or ("/" + fragment + "/") in ("/" + rel):
            return True
    return False


def sources(root: Path):
    for scan_root in SCAN_ROOTS:
        base = root / scan_root
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in SOURCE_EXTS or not path.is_file():
                continue
            if is_excluded(path, root):
                continue
            yield path


def mentions_guarding_group(path: Path) -> bool:
    """True if this file, or the header/source it pairs with, uses a scoped_thread_group.

    The member is usually declared in the header and only used from the .cpp,
    so neither file alone answers the question.
    """
    for candidate in (path, path.with_suffix(".hpp"), path.with_suffix(".h"), path.with_suffix(".cpp")):
        if candidate.is_file() and GROUP_GUARD in candidate.read_text(encoding="utf-8", errors="replace"):
            return True
    return False


def findings(root: Path):
    for path in sources(root):
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").split("\n")
        except OSError as err:  # pragma: no cover - unreadable file
            print(f"warning: could not read {path}: {err}", file=sys.stderr)
            continue
        uses_guarding_group = mentions_guarding_group(path)
        for index, line in enumerate(lines):
            if not CREATION.search(line):
                continue
            if uses_guarding_group and GROUP_CREATION.search(line):
                continue
            window = "\n".join(lines[index:index + GUARD_WINDOW])
            if any(guard in window for guard in GUARDS):
                continue
            yield path.relative_to(root).as_posix(), index + 1, line.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="exit 1 if any unguarded thread creation is found")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent, help="repository root")
    args = parser.parse_args()

    found = list(findings(args.root.resolve()))
    for rel, line_no, text in found:
        print(f"{rel}:{line_no}: unguarded thread creation: {text}")

    if not found:
        print("All background threads are guarded.")
        return 0

    print()
    print(f"{len(found)} unguarded thread creation(s).")
    print("Start threads with threads::start_guarded_thread(), or wrap the body in")
    print("threads::run_guarded() / threads::run_io_context_guarded() - see")
    print("include/threads/guarded_thread.hpp. A site that genuinely cannot must be")
    print("added to ALLOWED in tools/guarded_threads.py with the reason.")
    return 1 if args.check else 0


if __name__ == "__main__":
    sys.exit(main())
