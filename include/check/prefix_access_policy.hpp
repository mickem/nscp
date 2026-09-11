// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string.hpp>
#include <check/access_policy.hpp>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// The hierarchical variant of check::access::policy, for arguments which name a
// node in a separator-delimited namespace rather than a file: a registry key
// (`HKLM\SOFTWARE\MyApp`, separator `\`) or an event log channel
// (`Microsoft-Windows-Sysmon/Operational`, separator `/`).
//
// It exists because the plain policy matches a value against a glob and nothing
// else, which is the wrong shape here. An operator allowing `HKLM\SOFTWARE\App`
// means that key *and its subtree*; writing `HKLM\SOFTWARE\App*` to get it would
// also match `HKLM\SOFTWARE\AppEvil`, and writing `HKLM\SOFTWARE\App\*` would
// miss the key itself. So an entry with no wildcard matches the value exactly or
// as a prefix ending on a separator - never mid-segment - and an entry with a
// wildcard is matched as a glob, for the cases where that is what you want.
//
// Unlike the path variant there is nothing to resolve against a filesystem:
// these namespaces have no `..` and no links. What they do have is alternate
// spellings (`HKLM` and `HKEY_LOCAL_MACHINE` name the same hive), so a
// normaliser can be supplied; it is applied to both the allow-list entries and
// the value being tested, so the two are compared in the same spelling.

namespace check {
namespace access {

class prefix_policy {
 public:
  typedef std::function<std::string(const std::string &)> normalizer;

  prefix_policy(std::string noun, std::string nouns, std::string settings_path, const char separator, normalizer normalize = normalizer())
      : base_(std::move(noun), std::move(nouns), std::move(settings_path)), separator_(separator), normalize_(std::move(normalize)) {}

  // --- configuration -------------------------------------------------------

  void set_mode(const std::string &value) { base_.set_mode(value); }
  void add_predefined(const std::string &name, const std::string &value) { base_.add_predefined(name, value); }
  void clear_predefined() { base_.clear_predefined(); }

  void set_allow_list(const std::string &value) {
    entries_.clear();
    std::vector<std::string> raw;
    boost::algorithm::split(raw, value, boost::algorithm::is_any_of(","));
    for (std::string &item : raw) {
      boost::algorithm::trim(item);
      if (item.empty()) continue;
      entries_.push_back(make_entry(normalize(item)));
    }
  }

  void reset() {
    base_.reset();
    entries_.clear();
  }

  // --- state ---------------------------------------------------------------

  mode get_mode() const { return base_.get_mode(); }
  bool is_restricted() const { return base_.is_restricted(); }
  const std::string &get_config_error() const { return base_.get_config_error(); }
  std::size_t allow_list_size() const { return entries_.size(); }

  bool matches_allow_list(const std::string &value) const {
    const std::string subject = normalize(value);
    for (const entry &e : entries_) {
      if (e.is_glob) {
        if (boost::regex_match(subject, e.pattern)) return true;
      } else if (is_at_or_under(e.text, subject)) {
        return true;
      }
    }
    return false;
  }

  // --- resolution ----------------------------------------------------------

  decision resolve(const std::string &token) const {
    std::string configured;
    if (base_.lookup_predefined(token, configured)) return decision::accept(configured);

    if (!base_.get_config_error().empty()) return decision::refuse(base_.get_config_error());

    switch (base_.get_mode()) {
      case mode::any:
        return decision::accept(token);
      case mode::allowed:
        if (matches_allow_list(token)) return decision::accept(token);
        return decision::refuse(refusal(token, "it is not in 'allowed " + base_.get_nouns() + "'"));
      case mode::predefined:
      default:
        return decision::refuse(
            refusal(token, "'" + base_.get_noun() + " access' is set to predefined, so only a configured " + base_.get_noun() + " name may be used"));
    }
  }

 private:
  struct entry {
    bool is_glob;
    std::string text;      // literal entries, already normalised
    boost::regex pattern;  // glob entries
    entry() : is_glob(false) {}
  };

  std::string normalize(const std::string &value) const { return normalize_ ? normalize_(value) : value; }

  entry make_entry(const std::string &item) const {
    entry e;
    e.is_glob = item.find('*') != std::string::npos || item.find('?') != std::string::npos;
    e.text = trim_separators(item);
    if (e.is_glob) e.pattern = boost::regex(glob_to_regex(e.text), boost::regex::perl | boost::regex::icase);
    return e;
  }

  std::string trim_separators(std::string s) const {
    while (s.size() > 1 && s.back() == separator_) s.pop_back();
    return s;
  }

  // True when `value` is `prefix` itself or sits below it. The separator test
  // is what keeps `HKLM\SOFTWARE\App` from covering `HKLM\SOFTWARE\AppEvil`:
  // a prefix only counts when the next character ends the segment.
  bool is_at_or_under(const std::string &prefix, const std::string &value) const {
    const std::string subject = trim_separators(value);
    if (boost::algorithm::iequals(subject, prefix)) return true;
    if (subject.size() <= prefix.size()) return false;
    if (!boost::algorithm::iequals(subject.substr(0, prefix.size()), prefix)) return false;
    return subject[prefix.size()] == separator_;
  }

  std::string refusal(const std::string &token, const std::string &why) const {
    return "Refusing " + base_.get_noun() + " '" + token + "': " + why + " (see [" + base_.get_settings_path() + "] in the configuration)";
  }

  policy base_;
  char separator_;
  normalizer normalize_;
  std::vector<entry> entries_;
};

}  // namespace access
}  // namespace check
