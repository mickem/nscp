// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <check/path_access_policy.hpp>

#include <functional>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace check_disk_write_command {

// Outcome of one write test (create, write, flush-to-disk, read back, delete).
struct write_result {
  std::string path;          // the test file
  long long size = 0;        // bytes written (and read back)
  long long write_time = 0;  // ms spent creating, writing and flushing
  long long read_time = 0;   // ms spent reading back and verifying
  long long total_time = 0;  // ms for the full cycle including the delete
  std::string issues;        // problems found (empty when the test succeeded)
};

// Perform the actual write test against the filesystem. Refuses to touch a
// file that already exists; always tries to delete the file it created, even
// when the write or read-back failed.
write_result perform_write_test(const std::string &path, long long size);

typedef std::function<write_result(const std::string &path, long long size)> write_tester;

// Evaluate a write test using the given tester. Exposed (with an injectable
// tester) for unit testing the option parsing, filtering and rendering.
void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const write_tester &tester, const check::access::path_policy &access);

// `access` decides which paths the caller may name; see
// docs/docs/concepts/check-access.md. It is passed in rather than looked up so
// the gate can be unit tested without a live module.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::path_policy &access);

}  // namespace check_disk_write_command
