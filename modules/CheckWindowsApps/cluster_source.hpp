// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace check_cluster {
enum class object_kind { group, resource, node, network };

// Values belong to the snapshot, never to the lifetime of a native handle.
struct record {
  std::string name;
  std::string owner;
  std::string group;
  std::string type;
  long long state_id = -1;
};

using source = std::function<std::vector<record>(object_kind)>;
std::vector<record> fetch_local(object_kind kind);
std::string state_name(object_kind kind, long long state);
}  // namespace check_cluster
