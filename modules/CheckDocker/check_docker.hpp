// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>

// default_docker_endpoint / is_local_docker_endpoint live in
// docker_endpoint.hpp so they can be unit-tested without this module's
// protobuf and filter dependencies.

namespace docker_checks {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace docker_checks