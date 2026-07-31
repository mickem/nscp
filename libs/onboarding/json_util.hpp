// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <onboarding/onboarding.hpp>
#include <string>

// Internal helpers shared by the onboarding/sync translation units.
namespace onboarding {
namespace detail {

// Copy a JSON string in full. as_string().c_str() looks equivalent but
// truncates at the first embedded nul, which would turn a hostile
// "id\0../../etc" into a harmless looking "id" that passes validation and a
// PEM with a nul in it into a silently different PEM. Keep every byte and let
// the validators reject what they do not like.
inline std::string to_string(const boost::json::string &value) { return std::string(value.data(), value.size()); }

inline std::string require_string(const boost::json::object &object, const char *key, const char *context) {
  const boost::json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_string() || value->as_string().empty()) {
    throw onboarding_error(std::string(context) + " is missing " + key, false);
  }
  return to_string(value->as_string());
}

inline std::string optional_string(const boost::json::object &object, const char *key, const std::string &fallback) {
  const boost::json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_string() || value->as_string().empty()) {
    return fallback;
  }
  return to_string(value->as_string());
}

}  // namespace detail
}  // namespace onboarding
