// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

namespace process {

// The path to hand CreateProcess as lpApplicationName, given argv[0] of a
// configured command and the directory the launcher runs the child in
// (exec_arguments::root_path, which is ${base-path}).
//
// lpApplicationName and lpCurrentDirectory are resolved against *different*
// directories, and that is the whole reason this exists. lpCurrentDirectory
// sets the working directory of the child; a relative lpApplicationName is
// resolved against the working directory of the **calling** process, which for
// a Windows service is C:\Windows\System32 and for an interactive `nscp test`
// is wherever the operator happened to be standing. So the conventional
// `command = scripts\check_foo.bat` found its script only when the agent had
// been started from the installation directory.
//
// The legacy single-string form (lpApplicationName NULL, the module name
// parsed out of the command line) does not have this problem - that lookup
// honours lpCurrentDirectory - so before argv-isolation existed the relative
// form worked from anywhere. Rooting argv[0] here is what restores parity, and
// it is a fix rather than a policy change: nothing that already worked starts
// resolving somewhere else, and ${scripts} is ${exe-path}/scripts on Windows,
// so `scripts\check_foo.bat` joined onto ${base-path} names the same file the
// token does.
//
// Left alone:
//   * a path that names a root of its own (`C:\tools\x.exe`, `\srv\share\x.exe`,
//     and the drive-relative `C:x.exe` / root-relative `\x.exe`) - the operator
//     said where it is;
//   * a bare file name with no directory component (`cmd.exe`,
//     `powershell.exe`) - that is a request for the system's own executable
//     search, and rooting it at the installation directory would break every
//     wrapping that leans on PATH;
//   * an empty argv[0], or an empty root, which leaves nothing to root at.
std::string resolve_application_path(const std::string& root_path, const std::string& argv0);

// Quote a single argument for inclusion in a Windows command line so that
// CommandLineToArgvW round-trips it back to the original byte sequence.
//
// The rules (per Microsoft docs and the documented MSVCRT parser behaviour):
//   * If the argument is empty or contains space, tab, newline, or `"`, wrap
//     the whole argument in double quotes.
//   * Inside the quotes, every embedded `"` must be escaped as `\"`.
//   * A run of `\` immediately before a `"` (including the closing `"`) must
//     itself be doubled, so the parser sees the right number of literal
//     backslashes plus the literal `"`.
//   * `\` not adjacent to a `"` is left alone.
std::wstring quote_argv_w(const std::wstring& arg);

// Build a single Windows command line out of a UTF-8 argv. argv[0] is treated
// the same as the rest - call sites that want to lock the executable should
// also pass argv[0] as `lpApplicationName` to CreateProcess so that
// CreateProcess does not re-tokenise the first whitespace-bounded prefix.
std::wstring build_command_line_w(const std::vector<std::string>& argv);

}  // namespace process
