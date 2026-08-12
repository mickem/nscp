// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

#include "ad_replication_filter.hpp"

namespace ad_replication_source {

// Fetch the inbound replication neighbors of `server` (empty = the local
// machine) via DsBind/DsReplicaGetInfo. Returns false on failure with `error`
// set; `not_a_dc` is true when the failure means the target does not run the
// directory service (the common "checked a non-DC" case) rather than a
// replication problem.
bool fetch(const std::string &server, std::vector<ad_replication_filter::filter_obj_ptr> &out, std::string &error, bool &not_a_dc);

}  // namespace ad_replication_source
