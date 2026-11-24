// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <streambuf>
#include <string>

namespace check_logfile {

// Open a log file named by a caller for reading, in binary mode and seekable.
// Returns nullptr and sets `error` to a message naming the path when it cannot
// be read. Wrap the result in a std::istream.
//
// On Windows (log_file_win.cpp) the path is opened with CreateFileW, which:
//   * passes SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, so a path naming
//     a named pipe (\\.\pipe\x, \\host\pipe\x) cannot hand the pipe's server an
//     impersonation token for the agent's account - std::ifstream opens the
//     same path with no QoS flags, which does;
//   * refuses any handle that is not a disk file (pipes, consoles, devices),
//     which would otherwise also block the check in ReadFile;
//   * shares read, write and delete, so a log the writer holds open is read
//     without getting in its way;
//   * takes the UTF-8 path as UTF-16, so names outside the ANSI code page work.
//
// Elsewhere (log_file_unix.cpp) it is a std::filebuf, as before.
std::unique_ptr<std::streambuf> open_log_file(const std::string &path, std::string &error);

#ifdef _WIN32
namespace detail {
// The CreateFileW call open_log_file makes, before the file-type check: a
// HANDLE (as void*, INVALID_HANDLE_VALUE on failure) the caller closes. Only
// exposed so the unit test can hold a pipe connection open and read the
// impersonation level its server is given.
void *open_for_read(const std::string &path);
}  // namespace detail
#endif

}  // namespace check_logfile
