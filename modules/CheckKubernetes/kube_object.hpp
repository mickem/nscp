// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// What every API object has in common for the checks: its three top-level
// sections, its age, and the `created` / `age` keywords.

#include <boost/json.hpp>
#include <check/duration_keyword.hpp>
#include <ctime>
#include <memory>
#include <parsers/where.hpp>
#include <str/rfc3339.hpp>
#include <string>

#include "kube_json.hpp"

namespace kube_checks {

// The metadata, spec and status of an API object, with an empty object
// standing in for a section the server left out, so the record builders
// read fields without null checks.
struct api_object {
  const boost::json::object &metadata;
  const boost::json::object &spec;
  const boost::json::object &status;

  explicit api_object(const boost::json::object &o) : metadata(section(o, "metadata")), spec(section(o, "spec")), status(section(o, "status")) {}

  std::string name() const { return get_str(metadata, "name"); }
  std::string ns() const { return get_str(metadata, "namespace"); }

 private:
  static const boost::json::object &section(const boost::json::object &o, const char *key) {
    static const boost::json::object empty;
    const boost::json::object *s = get_obj(o, key);
    return s ? *s : empty;
  }
};

// creationTimestamp as the two keywords report it: seconds since (-1 when
// unknown) and the creation instant as unix time (0 when unknown).
struct object_age {
  long long age = -1;
  long long created = 0;
};

inline object_age age_of(const boost::json::object &metadata) {
  object_age out;
  out.age = str::seconds_since_rfc3339(get_str(metadata, "creationTimestamp"));
  if (out.age >= 0) out.created = static_cast<long long>(std::time(nullptr)) - out.age;
  return out;
}

// Register `created` (a date) and `age` (seconds, taking duration units in
// thresholds) on a record type. `event` reads as "the pod was created" or
// "the node joined the cluster".
template <class TObject, class TRegistry>
void register_age_keywords(TRegistry &registry, long long (TObject::*get_created)() const, long long (TObject::*get_age)() const, const std::string &event) {
  registry.add_int_var("created", parsers::where::type_date, get_created, "When " + event + " (date)").no_perf();
  static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;
  registry.add_int_var("age", type_custom_age, get_age, "Seconds since " + event + ", -1 when unknown (supports units, e.g. age < 1h)").no_perf();
  registry.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<TObject>>);
}

}  // namespace kube_checks
