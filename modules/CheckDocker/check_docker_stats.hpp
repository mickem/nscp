// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include "check_docker.hpp"

namespace docker_checks {

// Check per-container resource usage (CPU%, memory vs limit) via
// GET /containers/{id}/stats?stream=false.
void check_stats(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                 PB::Commands::QueryResponseMessage::Response *response, const fetcher_factory &make_fetcher);

}  // namespace docker_checks
