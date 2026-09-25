// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Tolerant boost::json accessors shared by the CheckKubernetes helpers. Kept
// free of any agent dependency so the header-only helpers (and their tests)
// need nothing but boost::json.

#include <boost/json.hpp>
#include <string>

namespace kube_checks {

// --- tolerant JSON accessors -------------------------------------------------
//
// API objects vary by Kubernetes version and by what a controller has filled
// in; a missing or differently-typed field degrades to an empty value, never
// throws.

inline std::string get_str(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_string()) return std::string(p->as_string().c_str());
  }
  return "";
}

inline long long get_num(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_int64()) return p->as_int64();
    if (p->is_uint64()) return static_cast<long long>(p->as_uint64());
    if (p->is_double()) return static_cast<long long>(p->as_double());
  }
  return 0;
}

inline bool get_bool(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_bool()) return p->as_bool();
  }
  return false;
}

// Nested object access; nullptr when absent or not an object.
inline const boost::json::object *get_obj(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_object()) return &p->as_object();
  }
  return nullptr;
}

// Nested array access; nullptr when absent or not an array.
inline const boost::json::array *get_arr(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_array()) return &p->as_array();
  }
  return nullptr;
}

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
