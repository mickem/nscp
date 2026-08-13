// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include "check_docker.hpp"

namespace docker_checks {

// Check docker disk usage (images, containers, volumes, build cache and what
// a prune would reclaim) via GET /system/df.
void check_df(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
              const fetcher_factory &make_fetcher);

}  // namespace docker_checks
