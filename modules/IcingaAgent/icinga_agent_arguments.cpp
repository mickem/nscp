// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_arguments.hpp"

#include <algorithm>

#include "icinga_agent_json.hpp"
#include "icinga_agent_macros.hpp"

namespace json = boost::json;

namespace icinga_agent {

std::string strip_dashes(const std::string &key) {
  const std::size_t first = key.find_first_not_of('-');
  return first == std::string::npos ? std::string() : key.substr(first);
}

argument_spec parse_argument_spec(const std::string &name, const json::value &definition) {
  argument_spec spec;
  spec.key = strip_dashes(name);
  if (!definition.is_object()) {
    // `"warning" = "$warn$"`: the value alone. A null one is a flag, as a
    // `"value": null` inside a dictionary is.
    if (!definition.is_null()) spec.value = definition;
    return spec;
  }
  const json::object &attributes = definition.get_object();
  if (const json::value *key = json_util::find(attributes, "key")) spec.key = strip_dashes(json_util::to_text(*key));
  if (const json::value *order = json_util::find(attributes, "order")) spec.order = json_util::to_long(*order, 0);
  if (const json::value *skip = json_util::find(attributes, "skip_key")) spec.skip_key = json_util::is_truthy(*skip);
  if (const json::value *repeat = json_util::find(attributes, "repeat_key")) spec.repeat_key = json_util::is_truthy(*repeat);
  if (const json::value *required = json_util::find(attributes, "required")) spec.required = json_util::is_truthy(*required);
  if (const json::value *set_if = json_util::find(attributes, "set_if")) spec.set_if = *set_if;
  const json::value *value = json_util::find(attributes, "value");
  if (value && !value->is_null()) spec.value = *value;
  return spec;
}

bool argument_enabled(const argument_spec &spec, const json::object &macros) {
  if (!spec.set_if) return true;
  const boost::optional<json::value> condition = expand_value(spec.set_if.value(), macros);
  return condition && json_util::is_truthy(condition.value());
}

boost::optional<std::vector<std::string>> resolve_argument_values(const argument_spec &spec, const json::object &macros) {
  if (!spec.value) return std::vector<std::string>();  // a flag
  const boost::optional<json::value> resolved = expand_value(spec.value.value(), macros);
  if (!resolved) {
    if (spec.required) throw argument_error("required argument '" + spec.key + "' uses a macro the master did not send");
    return boost::none;
  }
  const json::value &value = resolved.value();
  if (json_util::is_empty(value)) return boost::none;
  std::vector<std::string> values;
  if (value.is_array()) {
    for (const json::value &element : value.get_array()) values.push_back(json_util::to_text(element));
  } else {
    values.push_back(json_util::to_text(value));
  }
  return values;
}

std::list<std::string> render_argument(const argument_spec &spec, const std::vector<std::string> &values) {
  std::list<std::string> tokens;
  if (values.empty()) {
    if (!spec.key.empty()) tokens.push_back(spec.key);  // a flag
    return tokens;
  }
  if (spec.skip_key || spec.key.empty()) {
    tokens.insert(tokens.end(), values.begin(), values.end());
  } else if (spec.repeat_key) {
    for (const std::string &v : values) tokens.push_back(spec.key + "=" + v);
  } else {
    std::string joined;
    for (const std::string &v : values) joined += (joined.empty() ? "" : ",") + v;
    tokens.push_back(spec.key + "=" + joined);
  }
  return tokens;
}

std::list<std::string> build_arguments(const json::object &arguments, const json::object &macros) {
  struct rendered {
    long order;
    std::string key;
    std::list<std::string> tokens;
  };
  std::vector<rendered> rendered_arguments;
  for (const auto &entry : arguments) {
    const argument_spec spec = parse_argument_spec(std::string(entry.key()), entry.value());
    if (!argument_enabled(spec, macros)) continue;
    const boost::optional<std::vector<std::string>> values = resolve_argument_values(spec, macros);
    if (!values) continue;
    std::list<std::string> tokens = render_argument(spec, values.value());
    if (tokens.empty()) continue;
    rendered_arguments.push_back(rendered{spec.order, spec.key, std::move(tokens)});
  }
  std::stable_sort(rendered_arguments.begin(), rendered_arguments.end(),
                   [](const rendered &l, const rendered &r) { return l.order != r.order ? l.order < r.order : l.key < r.key; });
  std::list<std::string> out;
  for (rendered &r : rendered_arguments) out.splice(out.end(), r.tokens);
  return out;
}

}  // namespace icinga_agent
