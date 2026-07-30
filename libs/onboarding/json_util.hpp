// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <onboarding/onboarding.hpp>
#include <string>

// Internal helpers shared by the onboarding/sync translation units.
namespace onboarding {
namespace detail {

inline std::string require_string(const boost::json::object &object, const char *key, const char *context) {
  const boost::json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_string() || value->as_string().empty()) {
    throw onboarding_error(std::string(context) + " is missing " + key, false);
  }
  return std::string(value->as_string().c_str());
}

inline std::string optional_string(const boost::json::object &object, const char *key, const std::string &fallback) {
  const boost::json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_string() || value->as_string().empty()) {
    return fallback;
  }
  return std::string(value->as_string().c_str());
}

}  // namespace detail
}  // namespace onboarding
