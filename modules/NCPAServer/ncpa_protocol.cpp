// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_protocol.hpp"

#include <Helpers.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <str/constant_time.hpp>
#include <str/utils.hpp>

namespace json = boost::json;

namespace ncpa {

const char *const kScriptsModule = "CheckExternalScripts";

std::vector<std::string> split_path(const std::string &raw) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= raw.size()) {
    std::size_t end = raw.find('/', start);
    if (end == std::string::npos) end = raw.size();
    if (end > start) out.push_back(Mongoose::Helpers::url_decode(raw.substr(start, end - start), false));
    start = end + 1;
  }
  return out;
}

std::string form_value(const form_vector &form, const std::string &key, const std::string &fallback) {
  for (const auto &e : form) {
    if (e.first == key) return e.second;
  }
  return fallback;
}

bool form_has(const form_vector &form, const std::string &key) {
  return std::any_of(form.begin(), form.end(), [&key](const form_vector::value_type &e) { return e.first == key; });
}

std::vector<std::string> form_values(const form_vector &form, const std::string &key) {
  std::vector<std::string> out;
  for (const auto &e : form) {
    if (e.first == key) out.push_back(e.second);
  }
  return out;
}

std::vector<std::string> split_args(const std::string &value) {
  std::vector<std::string> out;
  str::utils::parse_prompt_command(value, out, false);
  return out;
}

token_result check_token(const std::string &given, const std::string &primary, const std::string &backup) {
  if (primary.empty()) return token_result::not_configured;
  // An empty token never matches, even against an empty backup.
  const bool primary_ok = str::constant_time_eq(given, primary);
  const bool backup_ok = !backup.empty() && str::constant_time_eq(given, backup);
  if (given.empty()) return token_result::rejected;
  return (primary_ok || backup_ok) ? token_result::accepted : token_result::rejected;
}

bool plugin_policy::parse(const std::string &value, plugin_policy &out, std::string &error) {
  const std::string trimmed = boost::algorithm::trim_copy(value);
  const std::string lower = boost::algorithm::to_lower_copy(trimmed);
  out = plugin_policy();
  if (lower == "any" || lower == "*") {
    out.mode = mode_type::any;
    return true;
  }
  if (lower == "scripts") {
    out.mode = mode_type::scripts;
    return true;
  }
  out.mode = mode_type::list;
  std::vector<std::string> parts;
  boost::algorithm::split(parts, lower, boost::algorithm::is_any_of(","));
  for (std::string &p : parts) {
    boost::algorithm::trim(p);
    if (!p.empty()) out.names.insert(p);
  }
  if (out.names.empty()) {
    error = "'plugins' is empty: expected 'any', 'scripts' or a comma-separated list of query names";
    return false;
  }
  return true;
}

bool plugin_policy::allows(const std::string &name, const std::string &module) const {
  switch (mode) {
    case mode_type::any:
      return true;
    case mode_type::scripts:
      return boost::algorithm::iequals(module, kScriptsModule);
    case mode_type::list:
      return names.count(boost::algorithm::to_lower_copy(name)) > 0;
  }
  return false;
}

std::string plugin_policy::to_string() const {
  switch (mode) {
    case mode_type::any:
      return "any";
    case mode_type::scripts:
      return "scripts";
    case mode_type::list:
      return boost::algorithm::join(names, ",");
  }
  return "";
}

std::string nagios_output(const std::string &message, const std::string &perf) {
  std::string text = perf.empty() ? message : message + "|" + perf;
  boost::algorithm::replace_all(text, "\r\n", "\n");
  boost::algorithm::replace_all(text, "\r", "\n");
  boost::algorithm::trim(text);
  return text;
}

bool is_truthy(const std::string &value) {
  const std::string v = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(value));
  return !(v.empty() || v == "0" || v == "false" || v == "no" || v == "off");
}

std::string error_body(const std::string &message) {
  json::object o;
  o["error"] = message;
  return json::serialize(o);
}

std::string check_body(const int returncode, const std::string &stdout_text) {
  json::object o;
  o["returncode"] = returncode;
  o["stdout"] = stdout_text;
  return json::serialize(o);
}

std::string list_body(const std::string &node, const std::vector<std::string> &names) {
  json::array a;
  for (const std::string &n : names) a.emplace_back(n);
  json::object o;
  o[node] = a;
  return json::serialize(o);
}

std::string missing_node_body(const std::string &full_path, const std::string &node_type, const std::string &name) {
  json::object inner;
  inner["path"] = full_path;
  inner["code"] = 100;
  inner["message"] = "The " + node_type + " requested does not exist.";
  inner[node_type] = name;
  json::object o;
  o["error"] = inner;
  return json::serialize(o);
}

std::string missing_node_check_body(const std::string &node_type, const std::string &name) {
  std::string text = "UNKNOWN: The " + node_type + " (" + name + ") requested does not exist.";
  // A '|' would start the performance data on the Nagios side.
  boost::algorithm::replace_all(text, "|", "/");
  return check_body(3, text);
}

}  // namespace ncpa
