// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <string>
#include <vector>

namespace PDH {

// instance name -> (counter name -> formatted value)
typedef std::map<std::string, std::map<std::string, double> > object_instance_values;

// Collect the given counters for every instance of a multi-instance
// performance object (e.g. "APP_POOL_WAS", "Web Service") in one query.
// Counter names are the English names; resolution falls back through the
// localized/English/index strategies. `double_sample` waits a second between
// two collections so rate counters (".../sec", "% ...") carry real values —
// without it they read 0. `_Total` style aggregate instances are skipped.
// Throws PDH::pdh_exception when the object/counters cannot be resolved
// (typically: the role providing them is not installed).
object_instance_values gather_object_instances(const std::string &object, const std::vector<std::string> &counters, bool double_sample);

// Collect the given counters of a single-instance performance object
// (e.g. "Terminal Services"). Returns counter name -> formatted value.
// Throws PDH::pdh_exception like gather_object_instances.
std::map<std::string, double> gather_object_values(const std::string &object, const std::vector<std::string> &counters, bool double_sample);

// The formatted value of one counter from a gathered map, or 0.0 when the
// counter is missing (a counter the object does not carry reads as 0 rather
// than as an error, matching how per-value formatting failures are reported).
inline double value_of(const std::map<std::string, double> &values, const std::string &counter) {
  const auto it = values.find(counter);
  return it == values.end() ? 0.0 : it->second;
}

}  // namespace PDH
