// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Tolerant boost::json accessors for payloads whose shape varies by version
// and configuration (the docker daemon, the Kubernetes API server, ...): a
// missing or differently-typed field degrades to an empty value, never
// throws. Pull them into a module's own namespace with using-declarations.

#include <boost/json.hpp>
#include <string>

namespace json_accessors {

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

}  // namespace json_accessors
