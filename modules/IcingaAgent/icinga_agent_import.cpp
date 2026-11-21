// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_import.hpp"

#include <regex>

namespace icinga_agent {

std::string strip_icinga_comments(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (c == '"') {
      // A string, to its closing quote; a backslash escapes the next character.
      out += c;
      ++i;
      while (i < text.size()) {
        out += text[i];
        if (text[i] == '\\' && i + 1 < text.size()) {
          out += text[++i];
        } else if (text[i] == '"') {
          break;
        }
        ++i;
      }
      ++i;
    } else if (c == '#' || (c == '/' && i + 1 < text.size() && text[i + 1] == '/')) {
      while (i < text.size() && text[i] != '\n') ++i;
    } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      const std::size_t end = text.find("*/", i + 2);
      i = end == std::string::npos ? text.size() : end + 2;
      out += ' ';
    } else {
      out += c;
      ++i;
    }
  }
  return out;
}

std::string parse_node_name(const std::string &constants_conf) {
  static const std::regex pattern("const\\s+NodeName\\s*=\\s*\"([^\"]*)\"");
  const std::string text = strip_icinga_comments(constants_conf);
  std::smatch match;
  return std::regex_search(text, match, pattern) ? match[1].str() : std::string();
}

std::vector<icinga_endpoint> parse_endpoints(const std::string &zones_conf) {
  // An Endpoint's body holds attributes only, so it ends at the first brace.
  static const std::regex object("object\\s+Endpoint\\s+(?:\"([^\"]*)\"|([A-Za-z_][A-Za-z0-9_]*))\\s*\\{([^}]*)\\}");
  static const std::regex host("(?:^|[\\s;{])host\\s*=\\s*\"([^\"]*)\"");
  static const std::regex port("(?:^|[\\s;{])port\\s*=\\s*\"?([0-9]+)\"?");
  const std::string text = strip_icinga_comments(zones_conf);
  std::vector<icinga_endpoint> endpoints;
  for (std::sregex_iterator it(text.begin(), text.end(), object), end; it != end; ++it) {
    icinga_endpoint endpoint;
    endpoint.name = (*it)[1].str();
    endpoint.is_node_name = (*it)[2].str() == "NodeName";
    const std::string body = (*it)[3].str();
    std::smatch match;
    if (std::regex_search(body, match, host)) endpoint.host = match[1].str();
    if (std::regex_search(body, match, port)) endpoint.port = match[1].str();
    endpoints.push_back(endpoint);
  }
  return endpoints;
}

std::vector<icinga_endpoint> parent_endpoints(const std::vector<icinga_endpoint> &endpoints, const std::string &node_name) {
  std::vector<icinga_endpoint> parents;
  for (const icinga_endpoint &endpoint : endpoints) {
    if (endpoint.is_node_name || endpoint.name.empty() || endpoint.name == node_name) continue;
    parents.push_back(endpoint);
  }
  return parents;
}

}  // namespace icinga_agent
