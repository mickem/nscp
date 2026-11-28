// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <nscapi/nscapi_facts_helper.hpp>
#include <set>
#include <string>
#include <vector>

// The `pdh` fact set: the Windows performance counter catalog.
//
//   pdh.objects                  every performance object and its counters
//   pdh.instances.<object>       the instances one object has right now
//
// Both are on-demand sets (nscapi::facts::response::on_demand). The catalog is
// thousands of entries on an ordinary server, and it is the same on every
// host with the same roles installed: a document a fleet server stores per
// host is the wrong place for it. Instances are worse - processes, disks and
// network connections come and go by the second - so they are read for one
// object at a time, when someone is picking a counter.
//
// Header-only and platform-neutral: the enumeration itself is PDH's, and
// lives in CheckSystem (Windows). What is here is the shape of the answer and
// the handling of the one parameter, which is what a unit test can hold on
// any platform.
namespace pdh_facts {

constexpr const char *const set_pdh = "pdh";
constexpr const char *const id_objects = "pdh.objects";
constexpr const char *const id_instances = "pdh.instances";
// A list in a fact set holds at most 5000 entries (the document rules).
constexpr std::size_t max_entries = 5000;

// One performance object. Owns its strings: copied out of the PDH
// enumeration, which is gone by the time the set is serialised.
struct object {
  std::string name;
  std::vector<std::string> counters;
};

namespace detail {
inline std::vector<std::string> capped(const std::vector<std::string> &values, bool &truncated) {
  if (values.size() <= max_entries) return values;
  truncated = true;
  return std::vector<std::string>(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(max_entries));
}
}  // namespace detail

// `pdh.objects`: one record per performance object, in the order PDH
// enumerated them, each with its counters in that order too. The order is
// kept because `\<object>\<counter>` in this order is what
// `/api/v2/metadata/counters` has always returned.
//
// An object PDH reported without a name, or twice, is skipped: a record's id
// has to be there and has to be unique in its list.
inline void publish_objects(const std::vector<object> &objects, const std::time_t taken_at, nscapi::facts::response &out) {
  nscapi::facts::record_list list = out.set(set_pdh).list("objects");
  std::set<std::string> seen;
  std::size_t written = 0;
  bool truncated = false;
  for (const object &o : objects) {
    if (o.name.empty() || !seen.insert(o.name).second) continue;
    if (written == max_entries) {
      truncated = true;
      break;
    }
    list.record(o.name).strings("counters", detail::capped(o.counters, truncated));
    ++written;
  }
  if (truncated) {
    out.error(id_objects, "Performance counter catalog truncated: at most " + std::to_string(max_entries) + " objects, and counters per object, are reported");
  }
  out.gathered(set_pdh, taken_at);
}

// The object a caller asked the instances of, as PDH spells it - or empty
// when PDH has no such object.
//
// The parameter is remote input: the rest of a REST path. It is only ever
// compared against the names PDH itself enumerated, and what is then handed
// to PDH is the enumerated name, never the caller's string. Compared exactly:
// a counter picker is passing back a name it was given by `pdh.objects`.
inline std::string find_object(const std::vector<std::string> &known, const std::string &parameter) {
  const std::vector<std::string>::const_iterator it = std::find(known.begin(), known.end(), parameter);
  return it == known.end() ? std::string() : *it;
}

// `pdh.instances`: the object that was asked about and its instances, in the
// order PDH enumerated them. An object without instances (`Memory`) is an
// empty list, which is a different answer from an object PDH does not have.
inline void publish_instances(const std::string &object_name, const std::vector<std::string> &instances, const std::time_t taken_at,
                              nscapi::facts::response &out) {
  bool truncated = false;
  out.set(set_pdh).object("instances").value("object", object_name).strings("names", detail::capped(instances, truncated));
  if (truncated) {
    out.error(id_instances, "Instance list truncated: found " + std::to_string(instances.size()) + ", reporting " + std::to_string(max_entries));
  }
  out.gathered(set_pdh, taken_at);
}

}  // namespace pdh_facts
