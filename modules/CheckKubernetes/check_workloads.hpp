// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>

#include "kube_client.hpp"
#include "kube_settings.hpp"

namespace kube_checks {

// check_workloads: desired versus available replicas of deployments,
// statefulsets and daemonsets, normalised into one record shape.
// GET /apis/apps/v1/{deployments,statefulsets,daemonsets}.
void check_workloads(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                     PB::Commands::QueryResponseMessage::Response *response, const fetcher_factory &make_fetcher);

}  // namespace kube_checks
