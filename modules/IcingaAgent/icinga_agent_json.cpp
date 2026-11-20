// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_json.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace json = boost::json;

namespace icinga_agent {
namespace json_util {

namespace {
std::string double_to_text(const double d) {
  // Within the range a double holds integers exactly, print them as such.
  if (std::isfinite(d) && std::floor(d) == d && std::fabs(d) < 1e15) return std::to_string(static_cast<long long>(d));
  std::ostringstream ss;
  ss << std::setprecision(15) << d;
  return ss.str();
}
}  // namespace

const json::value *find(const json::object &obj, const char *key) {
  const auto it = obj.find(key);
  return it == obj.end() ? nullptr : &it->value();
}

std::string to_text(const json::value &v) {
  switch (v.kind()) {
    case json::kind::string:
      return std::string(v.get_string());
    case json::kind::bool_:
      return v.get_bool() ? "true" : "false";
    case json::kind::int64:
      return std::to_string(v.get_int64());
    case json::kind::uint64:
      return std::to_string(v.get_uint64());
    case json::kind::double_:
      return double_to_text(v.get_double());
    case json::kind::null:
      return "";
    default:
      return json::serialize(v);
  }
}

std::string text_field(const json::object &obj, const char *key) {
  const json::value *v = find(obj, key);
  return v ? to_text(*v) : std::string();
}

bool is_empty(const json::value &v) {
  switch (v.kind()) {
    case json::kind::null:
      return true;
    case json::kind::string:
      return v.get_string().empty();
    case json::kind::array:
      return v.get_array().empty();
    case json::kind::object:
      return v.get_object().empty();
    default:
      return false;
  }
}

bool is_truthy(const json::value &v) {
  switch (v.kind()) {
    case json::kind::bool_:
      return v.get_bool();
    case json::kind::int64:
      return v.get_int64() != 0;
    case json::kind::uint64:
      return v.get_uint64() != 0;
    case json::kind::double_:
      return v.get_double() != 0;
    case json::kind::string: {
      const json::string &s = v.get_string();
      return !(s.empty() || s == "0" || s == "false");
    }
    default:
      return !is_empty(v);
  }
}

long to_long(const json::value &v, const long fallback) {
  if (v.is_int64()) return static_cast<long>(v.get_int64());
  if (v.is_uint64()) return static_cast<long>(v.get_uint64());
  if (v.is_double()) return static_cast<long>(v.get_double());
  if (!v.is_string()) return fallback;
  const std::string s(v.get_string());
  char *end = nullptr;
  errno = 0;
  const long parsed = std::strtol(s.c_str(), &end, 10);
  if (s.empty() || errno != 0 || end == nullptr || *end != '\0') return fallback;
  return parsed;
}

}  // namespace json_util
}  // namespace icinga_agent
