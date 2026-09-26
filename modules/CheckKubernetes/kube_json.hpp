// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The shared tolerant accessors plus the two Kubernetes-shaped helpers the
// header-only pieces (and their tests) need, free of any agent dependency.

#include <boost/json.hpp>
#include <json/accessors.hpp>
#include <string>

namespace kube_checks {

using json_accessors::get_arr;
using json_accessors::get_bool;
using json_accessors::get_num;
using json_accessors::get_obj;
using json_accessors::get_str;

// The `metadata` object every API object carries (nullptr when malformed).
inline const boost::json::object *metadata_of(const boost::json::object &o) { return get_obj(o, "metadata"); }

// The `status` of the condition of the given type in a `status.conditions`
// list ("True", "False" or "Unknown"); empty when the condition is not
// reported.
inline std::string condition_status(const boost::json::object &status, const char *type) {
  if (const boost::json::array *conditions = get_arr(status, "conditions")) {
    for (const auto &c : *conditions) {
      if (!c.is_object()) continue;
      if (get_str(c.as_object(), "type") == type) return get_str(c.as_object(), "status");
    }
  }
  return "";
}

// True when the condition of the given type is reported with status "True".
inline bool condition_is_true(const boost::json::object &status, const char *type) { return condition_status(status, type) == "True"; }

}  // namespace kube_checks
