// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// A shared gate for the checks whose argument names *what data is read* rather
// than how it is judged: `check_logfile file=`, `check_wmi query=`,
// `check_pdh counter=`.
//
// Those arguments are open by design - the whole point of check_logfile is to
// read the log file you name - but where the caller is a remote monitoring
// server (NRPE with `allow arguments`, or REST) the agent runs as SYSTEM/root
// and the argument decides which of the machine's data comes back. An operator
// who only wants two log files watched has, until now, had no way to say so.
//
// This is that knob. Three modes, per check:
//
//   any         Anything the caller names is read. The default, and what every
//               release before this one did, so an upgrade changes nothing.
//   allowed     Only values matching `allowed <nouns>` are read.
//   predefined  Only names the operator defined in configuration are read; a
//               raw value from the caller is refused.
//
// Predefined entries are operator-authored, so they resolve in *every* mode,
// including `any`. That ordering is what lets a site name its checks first and
// tighten the mode afterwards without rewriting the monitoring server's
// commands.
//
// The path-shaped variant (file names, which need `..` and symlinks resolved
// before they can be matched) lives in check/path_access_policy.hpp.
//
// A policy is read by the check threads and rewritten by a settings reload,
// which calls loadModuleEx again on the live module while those threads run.
// Every accessor and every mutator therefore takes the lock, and a mutator
// replaces its part of the state in one step: the gate is never observed
// half-rebuilt, and a reload never opens it - `reset()` drops only the
// predefined entries, which the settings callbacks re-add, while the mode and
// the allow list keep their previous values until notify() writes the new ones
// over them.
//
// A plain mutex, not a shared_mutex: the XP build defines _WIN32_WINNT=0x0501
// and MSVC's <shared_mutex> is empty below Vista, because it is built on
// SRWLOCK (net/socket/allowed_hosts.hpp makes the same choice for the same
// reason). Nothing is lost by it - the critical sections are a mode read, a
// map lookup and a walk of a handful of patterns, so there was never a
// reader/writer win to have.

namespace check {
namespace access {

enum class mode { any, allowed, predefined };

inline std::string to_string(const mode m) {
  switch (m) {
    case mode::allowed:
      return "allowed";
    case mode::predefined:
      return "predefined";
    case mode::any:
    default:
      return "any";
  }
}

// Parse a configured mode. Returns false on an unrecognised value, which the
// caller turns into a refusal rather than a silent fall back to `any`: a typo
// in a security setting must not read as "no restriction".
inline bool parse_mode(const std::string &value, mode &out) {
  const std::string v = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(value));
  if (v.empty() || v == "any" || v == "all" || v == "unrestricted") {
    out = mode::any;
    return true;
  }
  if (v == "allowed" || v == "allow" || v == "list") {
    out = mode::allowed;
    return true;
  }
  if (v == "predefined" || v == "defined" || v == "named") {
    out = mode::predefined;
    return true;
  }
  return false;
}

// Translate a shell-style glob into a regular expression which matches it
// literally apart from `*` and `?`.
//
// file_helpers::patterns::glob_to_regexp escapes only `.`, which is fine for
// the file masks it was written for but not here: a PDH counter is
// `\Processor(_Total)\% Processor Time` and a WMI class can be `Win32_*`, so
// `\`, `(`, `)` and `+` all have to survive as literals. An unescaped `(`
// would silently turn an allow-list entry into a capture group and change what
// it matches.
inline std::string glob_to_regex(const std::string &glob) {
  std::string re;
  re.reserve(glob.size() * 2);
  for (const char c : glob) {
    switch (c) {
      case '*':
        re += ".*";
        break;
      case '?':
        re += '.';
        break;
      case '.':
      case '\\':
      case '+':
      case '^':
      case '$':
      case '(':
      case ')':
      case '[':
      case ']':
      case '{':
      case '}':
      case '|':
        re += '\\';
        re += c;
        break;
      default:
        re += c;
        break;
    }
  }
  return re;
}

// The outcome of resolving one caller-supplied token.
//
// `value` is what the check should actually use: the token itself when it was
// accepted verbatim, or the operator-configured expansion when the token named
// a predefined entry. `error` is operator-facing and names the rejected token
// and the setting which would permit it - never the contents of the allow
// list, which the caller has no business enumerating.
struct decision {
  bool allowed;
  std::string value;
  std::string error;

  decision() : allowed(false) {}

  static decision accept(std::string value) {
    decision d;
    d.allowed = true;
    d.value = std::move(value);
    return d;
  }
  static decision refuse(std::string error) {
    decision d;
    d.allowed = false;
    d.error = std::move(error);
    return d;
  }
};

// One check's policy: the mode, the allow list, and the predefined entries.
//
// `noun` is what a single value is ("file", "query", "counter") and `nouns`
// its plural, used to build both the error messages and the names of the
// settings quoted in them. `settings_path` is the section the keys live in, so
// an operator reading the error knows where to go.
class policy {
 public:
  policy(std::string noun, std::string nouns, std::string settings_path, const bool case_sensitive = false)
      : noun_(std::move(noun)), nouns_(std::move(nouns)), settings_path_(std::move(settings_path)), case_sensitive_(case_sensitive), mode_(mode::any) {}

  // A mutex is neither copyable nor movable, so copying a policy copies its
  // configuration under the source's lock and gives the copy a lock of its
  // own.
  policy(const policy &other)
      : noun_(other.noun_), nouns_(other.nouns_), settings_path_(other.settings_path_), case_sensitive_(other.case_sensitive_), mode_(mode::any) {
    std::lock_guard<std::mutex> lock(other.mutex_);
    mode_ = other.mode_;
    config_error_ = other.config_error_;
    patterns_ = other.patterns_;
    raw_patterns_ = other.raw_patterns_;
    predefined_ = other.predefined_;
  }
  policy &operator=(const policy &other) {
    if (this == &other) return *this;
    policy copy(other);
    std::lock_guard<std::mutex> lock(mutex_);
    noun_ = copy.noun_;
    nouns_ = copy.nouns_;
    settings_path_ = copy.settings_path_;
    case_sensitive_ = copy.case_sensitive_;
    mode_ = copy.mode_;
    config_error_ = copy.config_error_;
    patterns_.swap(copy.patterns_);
    raw_patterns_.swap(copy.raw_patterns_);
    predefined_.swap(copy.predefined_);
    return *this;
  }

  // --- configuration -------------------------------------------------------

  // A mode which does not parse is remembered as a configuration error and
  // refuses every value, so a typo fails closed and says so once per check
  // rather than quietly opening the gate.
  void set_mode(const std::string &value) {
    mode m = mode::any;
    std::string error;
    if (!parse_mode(value, m)) {
      error = "invalid '" + noun_ + " access' in [" + settings_path_ + "]: '" + value + "' (expected any, allowed or predefined)";
      m = mode::predefined;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    config_error_.swap(error);
    mode_ = m;
  }

  // The new list is built beside the old one and swapped in whole, so a check
  // running through a reload sees either the previous list or the new one and
  // never an empty or half-filled one.
  void set_allow_list(const std::string &value) {
    std::vector<boost::regex> patterns;
    std::vector<std::string> raw_patterns;
    std::vector<std::string> entries;
    boost::algorithm::split(entries, value, boost::algorithm::is_any_of(","));
    for (std::string &entry : entries) {
      boost::algorithm::trim(entry);
      if (entry.empty()) continue;
      boost::regex::flag_type flags = boost::regex::perl;
      if (!case_sensitive_) flags |= boost::regex::icase;
      try {
        patterns.emplace_back(glob_to_regex(entry), flags);
        raw_patterns.push_back(entry);
      } catch (const boost::regex_error &) {
        // glob_to_regex escapes every metacharacter, so this should not be
        // reachable; drop the entry rather than let a malformed one widen the
        // list by throwing out of settings notification.
      }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    patterns_.swap(patterns);
    raw_patterns_.swap(raw_patterns);
  }

  void add_predefined(const std::string &name, const std::string &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    predefined_[name] = value;
  }
  void clear_predefined() {
    std::lock_guard<std::mutex> lock(mutex_);
    predefined_.clear();
  }

  // Called at the top of loadModuleEx before the settings are re-read. The
  // predefined entries are the one thing the callbacks *append* to, so they
  // have to start from nothing or every reload doubles them (the reload rule
  // in CLAUDE.md). The mode and the allow list are *replaced* by their
  // callbacks and are left alone here: dropping them would open the gate for
  // every check which arrives between this call and settings.notify().
  void reset() { clear_predefined(); }

  // --- state ---------------------------------------------------------------

  mode get_mode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
  }
  const std::string &get_settings_path() const { return settings_path_; }
  const std::string &get_noun() const { return noun_; }
  const std::string &get_nouns() const { return nouns_; }
  bool is_restricted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_ != mode::any || !config_error_.empty();
  }
  std::string get_config_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_error_;
  }
  bool has_predefined(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return predefined_.find(name) != predefined_.end();
  }
  bool lookup_predefined(const std::string &name, std::string &out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::string, std::string>::const_iterator it = predefined_.find(name);
    if (it == predefined_.end()) return false;
    out = it->second;
    return true;
  }
  std::size_t allow_list_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return patterns_.size();
  }

  bool matches_allow_list(const std::string &value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return matches_allow_list_unlocked(value);
  }

  // --- resolution ----------------------------------------------------------

  // Resolve one caller-supplied token into the value the check should use.
  decision resolve(const std::string &token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::string, std::string>::const_iterator it = predefined_.find(token);
    if (it != predefined_.end()) return decision::accept(it->second);

    if (!config_error_.empty()) return decision::refuse(config_error_);

    switch (mode_) {
      case mode::any:
        return decision::accept(token);
      case mode::allowed:
        if (matches_allow_list_unlocked(token)) return decision::accept(token);
        return decision::refuse(refusal(token, "it is not in 'allowed " + nouns_ + "'"));
      case mode::predefined:
      default:
        return decision::refuse(refusal(token, "'" + noun_ + " access' is set to predefined, so only a configured " + noun_ + " name may be used"));
    }
  }

  // Check a value which is not itself the token - a WMI class extracted from a
  // query, a namespace - against a second allow list held by another policy.
  decision check_value(const std::string &value, const std::string &what) const {
    if (matches_allow_list(value)) return decision::accept(value);
    return decision::refuse("Refusing " + what + " '" + value + "': it is not in 'allowed " + nouns_ + "' in [" + settings_path_ + "]");
  }

 private:
  bool matches_allow_list_unlocked(const std::string &value) const {
    for (const boost::regex &re : patterns_) {
      if (boost::regex_match(value, re)) return true;
    }
    return false;
  }

  std::string refusal(const std::string &token, const std::string &why) const {
    return "Refusing " + noun_ + " '" + token + "': " + why + " (see [" + settings_path_ + "] in the configuration)";
  }

  std::string noun_;
  std::string nouns_;
  std::string settings_path_;
  bool case_sensitive_;
  mutable std::mutex mutex_;
  mode mode_;
  std::string config_error_;
  std::vector<boost::regex> patterns_;
  std::vector<std::string> raw_patterns_;
  std::map<std::string, std::string> predefined_;
};

}  // namespace access
}  // namespace check
