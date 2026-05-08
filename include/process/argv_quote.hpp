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

}  // namespace process
