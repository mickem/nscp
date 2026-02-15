#!/usr/bin/env python3
"""
Script to format C/C++ and CMake files.

USAGE:
    format-code.py          - Formats all relevant files in the project.
    format-code.py --changed - Formats only files that are changed (modified or
                               untracked) according to Git.

REQUIREMENTS:
    - clang-format: For C/C++ files.
    - gersemi: For CMake files.
    - git: Required for the --changed option.
"""

import argparse
import subprocess
import sys
from pathlib import Path
from typing import List, Set

# Directories to exclude from code formatting (C/C++ files)
CODE_EXCLUDE_DIRS: Set[str] = {
    "_deps",
    "cmake-build",
    "cmake-build-debug",
    "cmake-build-debug-wsl",
    "cmake-build-relwithdebinfo-visual-studio",
    "cmake-build-release",
    "managed",
    "miniz",
    "gtest",
    "CheckPowershell",
    "DotnetPlugins",
    "dotnet-plugin-api",
    "rust",
    "build",
}

# Files to exclude from code formatting
CODE_EXCLUDE_FILES: Set[str] = {
    "mongoose.c",
    "mongoose.h",
}

# Directories to exclude from CMake formatting
CMAKE_EXCLUDE_DIRS: Set[str] = {
    "gtest",
    "cmake-build",
    "cmake-build-debug",
    "cmake-build-debug-wsl",
    "cmake-build-release",
    "miniz",
    "vagrant",
    "rust",
}

# C/C++ file extensions
CODE_EXTENSIONS: Set[str] = {".h", ".hpp", ".c", ".cpp", ".cc", ".cxx"}

# CMake file patterns
CMAKE_PATTERNS: Set[str] = {"CMakeLists.txt"}
CMAKE_EXTENSIONS: Set[str] = {".cmake"}


def get_project_root() -> Path:
    """Get the project root directory (where .git is located)."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
        )
        return Path(result.stdout.strip())
    except subprocess.CalledProcessError:
        # Fallback to current directory
        return Path.cwd()


def is_excluded_path(file_path: Path, exclude_dirs: Set[str], exclude_files: Set[str] = None) -> bool:
    """Check if a file path should be excluded based on directory or file name."""
    parts = file_path.parts
    for part in parts:
        if part in exclude_dirs:
            return True
    if exclude_files and file_path.name in exclude_files:
        return True
    return False


def get_changed_files() -> List[Path]:
    """Get list of modified and untracked files from Git."""
    try:
        result = subprocess.run(
            ["git", "ls-files", "-m", "-o", "--exclude-standard"],
            capture_output=True,
            text=True,
            check=True,
        )
        files = [Path(f) for f in result.stdout.strip().split("\n") if f]
        return files
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to get changed files from Git: {e}", file=sys.stderr)
        sys.exit(1)


def find_all_files(root: Path, extensions: Set[str], patterns: Set[str] = None) -> List[Path]:
    """Find all files with given extensions or matching patterns."""
    files = []
    for ext in extensions:
        files.extend(root.rglob(f"*{ext}"))
    if patterns:
        for pattern in patterns:
            files.extend(root.rglob(pattern))
    return files


def filter_code_files(files: List[Path]) -> List[Path]:
    """Filter files to only include C/C++ source files."""
    return [
        f for f in files
        if f.suffix in CODE_EXTENSIONS
        and not is_excluded_path(f, CODE_EXCLUDE_DIRS, CODE_EXCLUDE_FILES)
    ]


def filter_cmake_files(files: List[Path]) -> List[Path]:
    """Filter files to only include CMake files."""
    return [
        f for f in files
        if (f.suffix in CMAKE_EXTENSIONS or f.name in CMAKE_PATTERNS)
        and not is_excluded_path(f, CMAKE_EXCLUDE_DIRS)
    ]


def format_with_clang_format(files: List[Path]) -> bool:
    """Format C/C++ files using clang-format."""
    if not files:
        print("No C/C++ files to format.")
        return True

    print(f"\nFormatting {len(files)} C/C++ file(s)...")
    success = True
    for file in files:
        print(f"  {file}")
        try:
            subprocess.run(["clang-format", "-i", str(file)], check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error: clang-format failed on {file}: {e}", file=sys.stderr)
            success = False
        except FileNotFoundError:
            print("Error: clang-format not found. Please install clang-format.", file=sys.stderr)
            return False
    return success


def format_with_gersemi(files: List[Path]) -> bool:
    """Format CMake files using gersemi."""
    if not files:
        print("No CMake files to format.")
        return True

    print(f"\nFormatting {len(files)} CMake file(s)...")
    success = True
    for file in files:
        print(f"  {file}")
        try:
            subprocess.run(["gersemi", "--definitions", "build/cmake", "-i", str(file)], check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error: gersemi failed on {file}: {e}", file=sys.stderr)
            success = False
        except FileNotFoundError:
            print("Error: gersemi not found. Please install gersemi (pip install gersemi).", file=sys.stderr)
            return False
    return success


def main():
    parser = argparse.ArgumentParser(
        description="Format C/C++ and CMake files in the project."
    )
    parser.add_argument(
        "--changed",
        action="store_true",
        help="Only format files that are changed (modified or untracked) according to Git.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check if files are formatted without modifying them.",
    )
    args = parser.parse_args()

    project_root = get_project_root()
    print(f"Project root: {project_root}")

    if args.changed:
        print("Formatting changed files based on Git status...")
        all_files = get_changed_files()
        # Convert to absolute paths
        all_files = [project_root / f for f in all_files]
    else:
        print("Formatting all project files...")
        code_files = find_all_files(project_root, CODE_EXTENSIONS)
        cmake_files = find_all_files(project_root, CMAKE_EXTENSIONS, CMAKE_PATTERNS)
        all_files = code_files + cmake_files

    # Make paths relative to project root for cleaner output
    all_files = [f.relative_to(project_root) if f.is_absolute() else f for f in all_files]

    code_files = filter_code_files(all_files)
    cmake_files = filter_cmake_files(all_files)

    print(f"\nFound {len(code_files)} C/C++ file(s) and {len(cmake_files)} CMake file(s) to format.")

    # Format files
    code_success = format_with_clang_format(code_files)
    cmake_success = format_with_gersemi(cmake_files)

    if code_success and cmake_success:
        print("\nFormatting complete.")
        sys.exit(0)
    else:
        print("\nFormatting completed with errors.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

