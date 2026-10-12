// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/optional.hpp>
#include <settings/settings_validation.hpp>
#include <vector>

namespace settings {

namespace {

std::string lower(const std::string &s) { return boost::algorithm::to_lower_copy(s); }

// What a near miss is measured on: case, and the three ways people spell the
// gap in a multi-word name, do not count.
std::string normalize(const std::string &s) {
  std::string ret = lower(s);
  std::replace(ret.begin(), ret.end(), '_', ' ');
  std::replace(ret.begin(), ret.end(), '-', ' ');
  return ret;
}

// Optimal string alignment distance: Levenshtein plus adjacent transposition,
// so "scirpts" is one edit from "scripts" rather than two.
std::size_t edit_distance(const std::string &a, const std::string &b) {
  const std::size_t n = a.size(), m = b.size();
  std::vector<std::vector<std::size_t>> d(n + 1, std::vector<std::size_t>(m + 1, 0));
  for (std::size_t i = 0; i <= n; ++i) d[i][0] = i;
  for (std::size_t j = 0; j <= m; ++j) d[0][j] = j;
  for (std::size_t i = 1; i <= n; ++i) {
    for (std::size_t j = 1; j <= m; ++j) {
      const std::size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
      d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
    }
  }
  return d[n][m];
}

// How many edits still read as a typo of a name this long. Names of four
// characters or fewer get none: module sections are short acronyms one edit
// apart ([/settings/NSCA/client] next to [/settings/NSCP/client], "lua" and
// "log"), and a section for a module that is not loaded must not be told it
// meant a sibling's. Over every path the shipped modules register, this
// leaves no two sections within reach of each other.
std::size_t allowed_edits(const std::size_t length) {
  if (length <= 4) return 0;
  if (length <= 6) return 1;
  return 2;
}

// The candidate `name` is most plausibly a typo of, if any.
boost::optional<std::string> closest(const std::string &name, const std::list<std::string> &candidates) {
  const std::string n = normalize(name);
  boost::optional<std::string> best;
  std::size_t best_distance = 0;
  for (const std::string &c : candidates) {
    const std::string nc = normalize(c);
    const std::size_t distance = edit_distance(n, nc);
    if (distance > allowed_edits(nc.size())) continue;
    if (!best || distance < best_distance) {
      best = c;
      best_distance = distance;
    }
  }
  return best;
}

std::vector<std::string> split_path(const std::string &path) {
  std::vector<std::string> ret;
  boost::algorithm::split(ret, path, [](const char c) { return c == '/'; });
  return ret;
}

// A registered path that differs from `path` in exactly one segment, by a
// typo. Comparing per segment keeps the allowance tied to the name that was
// misspelled rather than to the length of the whole path.
boost::optional<std::string> closest_path(const std::string &path, const std::list<std::string> &registered) {
  const std::vector<std::string> segments = split_path(path);
  boost::optional<std::string> best;
  std::size_t best_distance = 0;
  for (const std::string &candidate : registered) {
    const std::vector<std::string> other = split_path(candidate);
    if (other.size() != segments.size()) continue;
    std::size_t differing = 0, distance = 0;
    bool near = true;
    for (std::size_t i = 0; i < segments.size() && near; ++i) {
      const std::string a = normalize(segments[i]), b = normalize(other[i]);
      if (a == b) continue;
      ++differing;
      distance = edit_distance(a, b);
      near = differing == 1 && distance <= allowed_edits(b.size());
    }
    if (!near) continue;
    if (!best || distance < best_distance) {
      best = candidate;
      best_distance = distance;
    }
  }
  return best;
}

std::string parent_of(const std::string &path) {
  const std::string::size_type pos = path.find_last_of('/');
  if (pos == std::string::npos || pos == 0) return "";
  return path.substr(0, pos);
}

// A typo above the section itself: [/settings/external scripts/scirpts/check]
// has no near miss of its own, because nothing is registered under the
// misspelled parent. Walk up through the ancestors nothing registered, and
// carry what lies below the one that is a near miss over to the suggestion.
// The walk stops at the first registered ancestor: above it the path is right.
boost::optional<std::string> closest_ancestor(const std::string &path, const std::map<std::string, const registry_snapshot::path_entry *> &by_path,
                                              const std::list<std::string> &registered) {
  for (std::string ancestor = parent_of(path); !ancestor.empty(); ancestor = parent_of(ancestor)) {
    if (by_path.find(lower(ancestor)) != by_path.end()) return boost::none;
    if (const auto near = closest_path(ancestor, registered)) return near.value() + path.substr(ancestor.size());
  }
  return boost::none;
}

validation_finding make(validation_finding::kind type, validation_finding::severity level, const inventory_section &section, const std::string &key,
                        const std::string &suggestion, const std::string &message) {
  validation_finding f;
  f.type = type;
  f.level = level;
  f.source = section.source;
  f.path = section.path;
  f.key = key;
  f.suggestion = suggestion;
  f.message = message;
  return f;
}

}  // namespace

validation_result validate_inventory(const inventory &inv, const registry_snapshot &registry, const bool complete_registry) {
  typedef validation_finding vf;
  // Both stores read paths and keys without regard to case, so a section
  // spelled [/Settings/External Scripts] is in use and must not be flagged.
  std::map<std::string, const registry_snapshot::path_entry *> by_path;
  std::list<std::string> registered_paths;
  for (const auto &p : registry.paths) {
    by_path[lower(p.first)] = &p.second;
    registered_paths.push_back(p.first);
  }
  const vf::severity unused_level = complete_registry ? vf::severity::warning : vf::severity::info;
  const std::string not_used_by = complete_registry ? "is not used by any installed module" : "is not used by any loaded module";

  validation_result ret;
  for (const inventory_section &section : inv) {
    if (section.path.empty()) {
      for (const std::string &key : section.keys) {
        ret.push_back(
            make(vf::kind::orphan_key, vf::severity::warning, section, key, "", "Key '" + key + "' appears before the first [section] header and is ignored."));
      }
      continue;
    }
    // An empty section configures nothing, so nothing in it is ignored: it
    // is listed, but it is no reason to warn. `nscp settings --update` wrote
    // an empty [settings] into every file it touched for years.
    const vf::severity misplaced_level = section.keys.empty() ? vf::severity::info : vf::severity::warning;
    if (section.path[0] != '/') {
      const std::string fixed = "/" + section.path;
      ret.push_back(make(vf::kind::relative_path, misplaced_level, section, "", fixed,
                         "Section [" + section.path + "] is ignored: settings paths start with '/'. Did you mean [" + fixed + "]?"));
      continue;
    }

    const auto it = by_path.find(lower(section.path));
    if (it == by_path.end()) {
      // An object under a path that takes user-named entries
      // ([/settings/external scripts/scripts/my_script]) is read by the
      // module that owns the parent; nothing to compare it against.
      const auto parent = by_path.find(lower(parent_of(section.path)));
      if (parent != by_path.end() && parent->second->any_key) continue;
      boost::optional<std::string> near = closest_path(section.path, registered_paths);
      if (!near) near = closest_ancestor(section.path, by_path, registered_paths);
      if (near) {
        ret.push_back(make(vf::kind::unknown_path, misplaced_level, section, "", near.value(),
                           "Section [" + section.path + "] is not a known settings path. Did you mean [" + near.value() + "]?"));
      } else {
        std::string message = "Section [" + section.path + "] " + not_used_by;
        if (!complete_registry) message += " (it may belong to a module that is not enabled)";
        ret.push_back(make(vf::kind::unused_path, unused_level, section, "", "", message + "."));
      }
      continue;
    }

    const registry_snapshot::path_entry &entry = *it->second;
    if (entry.any_key) continue;
    std::map<std::string, std::string> known;
    for (const std::string &k : entry.keys) known[lower(k)] = k;
    for (const std::string &key : section.keys) {
      if (known.find(lower(key)) != known.end()) continue;
      if (const auto near = closest(key, entry.keys)) {
        ret.push_back(make(vf::kind::unknown_key, vf::severity::warning, section, key, near.value(),
                           "Key '" + key + "' in [" + section.path + "] is not a known key. Did you mean '" + near.value() + "'?"));
      } else {
        std::string message = "Key '" + key + "' in [" + section.path + "] " + not_used_by;
        if (!complete_registry) message += " (it may belong to a module that is not enabled)";
        ret.push_back(make(vf::kind::unused_key, unused_level, section, key, "", message + "."));
      }
    }
  }
  return ret;
}

std::size_t count_warnings(const validation_result &result) {
  return static_cast<std::size_t>(
      std::count_if(result.begin(), result.end(), [](const validation_finding &f) { return f.level == validation_finding::severity::warning; }));
}

}  // namespace settings
