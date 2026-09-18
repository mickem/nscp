// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

namespace process {

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

// True when `program` names a Windows batch file (.bat or .cmd), case
// insensitively, ignoring any surrounding quotes.
//
// This is not cosmetic. CreateProcess cannot execute a batch file: given one it
// re-launches `cmd.exe /c <command line>`, and cmd.exe then parses that line by
// its own rules - which are not CommandLineToArgvW's. So for a batch target the
// argv isolation above buys much less than it looks: `%VAR%` still expands from
// the service environment, `^` still escapes, and a CR or LF inside an argument
// ends the statement so the rest is parsed as a fresh command, with no quote
// breakout needed. (This is the mechanism behind the BatBadBut class of
// vulnerabilities, CVE-2024-24576 and friends.) Callers that let a remote
// caller supply argument values must therefore hold those values to the
// stricter shell rules when this returns true.
bool is_batch_target(const std::string& program);

}  // namespace process
