/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/json.hpp>
#include <cctype>
#include <cstddef>
#include <list>
#include <nsclient/nsclient_exception.hpp>
#include <str/utf8.hpp>
#include <str/utils_no_boost.hpp>
#include <str/xtos.hpp>
#include <string>

// Pure-logic helpers for the CheckNSCP module. Kept header-only so that the
// gtest-based unit tests can exercise version parsing and the GitHub release
// payload parser without having to link the plugin glue.
namespace check_nscp_helpers {

struct nscp_version {
  int release;
  int major_version;
  int minor_version;
  int build;
  bool has_build;
  std::string date;

  nscp_version() : release(0), major_version(0), minor_version(0), build(0), has_build(false) {}
  nscp_version(const nscp_version &other) = default;
  nscp_version &operator=(const nscp_version &other) = default;
  explicit nscp_version(const std::string &v) : release(0), major_version(0), minor_version(0), build(0), has_build(false) {
    const str::utils::token v2 = str::utils::split2(v, " ");
    date = v2.second;
    std::list<std::string> vl = str::utils::split_lst(v2.first, ".");
    if (vl.empty() || vl.size() > 4) {
      throw nsclient::nsclient_exception("Failed to parse version: " + v);
    }
    if (!vl.empty()) {
      release = str::stox<int>(vl.front());
      vl.pop_front();
    }
    if (!vl.empty()) {
      major_version = str::stox<int>(vl.front());
      vl.pop_front();
    }
    if (!vl.empty()) {
      minor_version = str::stox<int>(vl.front());
      vl.pop_front();
    }
    if (!vl.empty()) {
      build = str::stox<int>(vl.front());
      has_build = true;
    }
  }
  std::string to_string() const {
    if (has_build) {
      return str::xtos(release) + "." + str::xtos(major_version) + "." + str::xtos(minor_version) + "." + str::xtos(build);
    }
    return str::xtos(release) + "." + str::xtos(major_version) + "." + str::xtos(minor_version);
  }
};

// Sanitize a GitHub tag name into something nscp_version can parse. Strips a
// leading "v" or "V" and truncates at the first non-version character (e.g.
// "0.6.5-rc1" -> "0.6.5"). Empty input is preserved so the caller can detect
// the parse failure further down.
inline std::string sanitize_tag(const std::string &tag) {
  std::string t = tag;
  if (!t.empty() && (t[0] == 'v' || t[0] == 'V')) t.erase(0, 1);
  std::size_t i = 0;
  while (i < t.size() && (std::isdigit(static_cast<unsigned char>(t[i])) || t[i] == '.')) ++i;
  t.resize(i);
  return t;
}

// Compare two parsed versions: returns negative if a < b, 0 if equal, positive
// if a > b. The build component is only considered when both versions report
// one.
inline int compare(const nscp_version &a, const nscp_version &b) {
  if (a.release != b.release) return a.release - b.release;
  if (a.major_version != b.major_version) return a.major_version - b.major_version;
  if (a.minor_version != b.minor_version) return a.minor_version - b.minor_version;
  if (a.has_build && b.has_build && a.build != b.build) return a.build - b.build;
  return 0;
}

// Parse the GitHub releases response body and extract the tag/url/published
// date of the chosen release. When include_prerelease is false, drafts and
// pre-releases are skipped. The body may be either a JSON array (when the
// caller hit /releases) or a single JSON object (when the caller hit
// /releases/latest); both are handled.
inline bool parse_releases_payload(const std::string &payload, bool include_prerelease, std::string &tag, std::string &url, std::string &published,
                                   std::string &error) {
  namespace json = boost::json;
  try {
    const json::value root = json::parse(payload);
    auto pick = [&](const json::object &obj) -> bool {
      const auto draft = obj.if_contains("draft");
      if (draft && draft->is_bool() && draft->as_bool()) return false;
      const auto pre = obj.if_contains("prerelease");
      const bool is_pre = pre && pre->is_bool() && pre->as_bool();
      if (is_pre && !include_prerelease) return false;
      const auto t = obj.if_contains("tag_name");
      if (!t || !t->is_string()) return false;
      tag = std::string(t->as_string().c_str());
      const auto u = obj.if_contains("html_url");
      if (u && u->is_string()) url = std::string(u->as_string().c_str());
      const auto p = obj.if_contains("published_at");
      if (p && p->is_string()) published = std::string(p->as_string().c_str());
      return true;
    };
    if (root.is_array()) {
      for (const json::value &v : root.as_array()) {
        if (!v.is_object()) continue;
        if (pick(v.as_object())) return true;
      }
      error = include_prerelease ? "no releases found" : "no stable releases found";
      return false;
    }
    if (root.is_object()) {
      if (pick(root.as_object())) return true;
      error = "release was filtered out (draft or pre-release)";
      return false;
    }
    error = "unexpected JSON shape in response";
    return false;
  } catch (const std::exception &e) {
    error = std::string("failed to parse JSON: ") + utf8::utf8_from_native(e.what());
    return false;
  }
}

}  // namespace check_nscp_helpers
