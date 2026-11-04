// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <check/path_access_policy.hpp>
#include <functional>
#include <nscapi/protobuf/command.hpp>

#include "largest_files_scan.hpp"

namespace check_largest_files_command {

typedef std::function<largest_files::scan_result(const largest_files::scan_request &)> scanner;

// The check against an explicit scanner, so the option handling and the
// rendering are unit tested without touching a disk.
void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const check::access::path_policy &access, const scanner &scan);

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::path_policy &access);

}  // namespace check_largest_files_command
