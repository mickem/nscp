// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <check/path_access_policy.hpp>

#include <nscapi/protobuf/command.hpp>

namespace check_single_file_command {
// `access` decides which paths the caller may name; see
// docs/docs/concepts/check-access.md. It is passed in rather than looked up so
// the gate can be unit tested without a live module.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::path_policy &access);
}
