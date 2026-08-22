// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

#include "ad_replication_filter.hpp"

namespace ad_replication_source {

// Fetch the inbound replication neighbors of `server` (empty = the local
// machine) via DsBind/DsReplicaGetInfo. `timeout_ms` bounds the reachability
// pre-check made before binding to a named remote server. Returns false on
// failure with `error` set; `not_a_dc` is true when the target is known not to
// be a domain controller (the common "checked a member server" case) rather
// than a domain controller that failed to answer.
bool fetch(const std::string &server, int timeout_ms, std::vector<ad_replication_filter::filter_obj_ptr> &out, std::string &error, bool &not_a_dc);

}  // namespace ad_replication_source
