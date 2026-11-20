// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_macros.hpp"

#include "icinga_agent_json.hpp"

namespace json = boost::json;

namespace icinga_agent {

boost::optional<std::string> sole_macro_name(const std::string &text) {
  if (text.size() < 3 || text.front() != '$' || text.back() != '$') return boost::none;
  // No `$` in between: "$a$$b$" is two macros, not one called "a$$b".
  if (text.find('$', 1) != text.size() - 1) return boost::none;
  return text.substr(1, text.size() - 2);
}

std::string macro_value_text(const json::value &value) {
  if (!value.is_array()) return json_util::to_text(value);
  std::string joined;
  for (const json::value &element : value.get_array()) {
    if (!joined.empty()) joined += ",";
    joined += json_util::to_text(element);
  }
  return joined;
}

boost::optional<json::value> expand_macros(const std::string &text, const json::object &macros) {
  if (const boost::optional<std::string> name = sole_macro_name(text)) {
    const json::value *value = json_util::find(macros, name.value().c_str());
    if (!value) return boost::none;
    return *value;
  }
  std::string out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t open = text.find('$', pos);
    if (open == std::string::npos) {
      out.append(text, pos, std::string::npos);
      break;
    }
    const std::size_t close = text.find('$', open + 1);
    if (close == std::string::npos) throw macro_syntax_error("unterminated macro in '" + text + "'");
    out.append(text, pos, open - pos);
    const std::string name = text.substr(open + 1, close - open - 1);
    if (name.empty()) {
      out += '$';  // `$$`
    } else {
      const json::value *value = json_util::find(macros, name.c_str());
      if (!value) return boost::none;
      out += macro_value_text(*value);
    }
    pos = close + 1;
  }
  return json::value(out);
}

boost::optional<json::value> expand_value(const json::value &value, const json::object &macros) {
  if (value.is_string()) return expand_macros(std::string(value.get_string()), macros);
  if (!value.is_array()) return value;
  json::array out;
  for (const json::value &element : value.get_array()) {
    const boost::optional<json::value> expanded = expand_value(element, macros);
    if (!expanded) return boost::none;
    out.push_back(expanded.value());
  }
  return json::value(std::move(out));
}

}  // namespace icinga_agent
