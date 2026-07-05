// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace nsclient {
namespace core {

// Policy decision point for "may subject X run object Y".
//
// `subject` identifies the caller as `module[:principal]`:
//   - "WEBServer"            -> module call, any principal
//   - "WEBServer:operator"   -> module call as named principal
//   - "NRPEServer:10.0.0.5"  -> module call as remote IP
//   - "*"                    -> wildcard (matches any subject)
//
// `object` identifies the call target as `module.command`:
//   - "CheckHelpers.check_ok" -> a specific command in a specific module
//   - "CheckHelpers.*"        -> any command in CheckHelpers
//   - "check_cpu"             -> a command in any module
//   - "*"                     -> wildcard
//
// Rules combine additively: a subject is allowed to call an object if ANY
// matching rule grants it. The class is intentionally simple and self
// contained so it can be tested without the rest of the core - the design
// doc spec (docs/design/core-permissions.md) is the source of truth.
class permissions {
 public:
  permissions() : enabled_(false), allow_exec_(true), log_denials_(true), log_allows_(false) {}

  void set_enabled(bool v) {
    std::lock_guard<std::mutex> lk(mutex_);
    enabled_ = v;
  }
  bool is_enabled() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return enabled_;
  }

  // Global exec toggle. Per-command policies in the rule table apply
  // to QUERIES only; exec_command is gated by this single switch. The
  // distinction exists because:
  //   - The remote-facing inbound surfaces (NRPE/NSCA/NRDP) only carry
  //     queries, so per-caller exec gating is mostly moot for them.
  //   - The internal exec chain (lua/python -> core_helper::exec_simple_command)
  //     does not propagate caller identity today, so a per-command exec
  //     policy decision degenerates to subject "*" and is unreliable
  //     anyway.
  //   - WEB exec endpoints are gated by WEB authentication separately.
  // Default is true (exec allowed) so enabling the policy system does
  // not silently break exec callers; operators who want a hard exec
  // lockdown flip it to false.
  void set_allow_exec(bool v) {
    std::lock_guard<std::mutex> lk(mutex_);
    allow_exec_ = v;
  }
  bool is_exec_allowed() const {
    std::lock_guard<std::mutex> lk(mutex_);
    // When the policy system is disabled, exec is always allowed (same
    // bypass as is_allowed). When enabled, the toggle decides.
    return !enabled_ || allow_exec_;
  }

  void set_log_denials(bool v) {
    std::lock_guard<std::mutex> lk(mutex_);
    log_denials_ = v;
  }
  bool should_log_denials() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return log_denials_;
  }

  void set_log_allows(bool v) {
    std::lock_guard<std::mutex> lk(mutex_);
    log_allows_ = v;
  }
  bool should_log_allows() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return log_allows_;
  }

  // Add a rule. `subject_pattern` is the subject side of the policy (one
  // key in [/settings/permissions/policies]); `objects_csv` is the
  // comma-separated list of object patterns. Whitespace around list
  // separators is stripped. Calling multiple times with the same subject
  // appends rather than replaces - rules are additive.
  void add_rule(const std::string& subject_pattern, const std::string& objects_csv) {
    std::lock_guard<std::mutex> lk(mutex_);
    add_rule_locked(subject_pattern, objects_csv);
  }

  // Drop all rules. Used on settings reload before re-registering the
  // policies tree. Other settings (enabled / log_*) survive because they
  // are set via separate keys that the registry rebinds before the
  // policies are re-added.
  void clear_rules() {
    std::lock_guard<std::mutex> lk(mutex_);
    rules_.clear();
  }

  std::size_t rule_count() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return rules_.size();
  }

  // The policy decision. `subject` is `module[:principal]` (use
  // make_subject below); `object` is `module.command` (use make_object).
  //
  // When the policy system is disabled (the rollout default), always
  // returns true. When enabled, the rules form a strict allow-list:
  // a call is permitted only if some rule grants it; unmatched subjects
  // are denied. (We previously had a `default = allow|deny` option, but
  // in an allow-only rule world `default = allow` was a no-op and just
  // confused operators; the only meaningful enforcement stance is "no
  // match -> deny". If we ever add deny-prefix rules, a `default`
  // option may be worth reintroducing then.)
  bool is_allowed(const std::string& subject, const std::string& object) const {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!enabled_) return true;
    for (const auto& rule : rules_) {
      if (!subject_matches(rule.subject, subject)) continue;
      for (const auto& obj_pattern : rule.objects) {
        if (object_matches(obj_pattern, object)) return true;
      }
    }
    return false;
  }

  // Format a subject string from the (module, principal) pair. Empty
  // principal -> bare module. Used by callers stamping the policy
  // decision; not user-visible.
  static std::string make_subject(const std::string& module, const std::string& principal) {
    if (principal.empty()) return module;
    return module + ":" + principal;
  }

  // Format an object string from the (module, command) pair. Used by the
  // plugin manager just before the decision point.
  static std::string make_object(const std::string& module, const std::string& command) {
    if (module.empty()) return command;
    return module + "." + command;
  }

 private:
  struct rule {
    std::string subject;               // pattern, may include * and ?
    std::vector<std::string> objects;  // each pattern, may include * and ?
  };

  // No-mutex variant for use from inside already-locked methods.
  void add_rule_locked(const std::string& subject_pattern, const std::string& objects_csv) {
    rule r;
    r.subject = subject_pattern;
    std::string token;
    for (const char c : objects_csv) {
      if (c == ',') {
        const std::string trimmed = trim(token);
        if (!trimmed.empty()) r.objects.push_back(trimmed);
        token.clear();
      } else {
        token.push_back(c);
      }
    }
    const std::string trimmed = trim(token);
    if (!trimmed.empty()) r.objects.push_back(trimmed);
    if (!r.objects.empty()) rules_.push_back(std::move(r));
  }

  static std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
  }

  // Glob match supporting `*` (any number of chars) and `?` (one char).
  // Case-insensitive on the assumption that command and module names are
  // ASCII (no Unicode case folding required).
  static bool glob(const std::string& pattern, const std::string& text) {
    const std::string p = boost::algorithm::to_lower_copy(pattern);
    const std::string t = boost::algorithm::to_lower_copy(text);
    return glob_impl(p.c_str(), t.c_str());
  }

  // Iterative glob matcher with backtracking on `*`. Standard textbook
  // shape; O(P*T) worst case.
  static bool glob_impl(const char* pattern, const char* text) {
    const char* p = pattern;
    const char* t = text;
    const char* star_p = nullptr;
    const char* star_t = nullptr;
    while (*t) {
      if (*p == '?' || *p == *t) {
        ++p;
        ++t;
      } else if (*p == '*') {
        star_p = p++;
        star_t = t;
      } else if (star_p) {
        p = star_p + 1;
        t = ++star_t;
      } else {
        return false;
      }
    }
    while (*p == '*') ++p;
    return *p == '\0';
  }

  // Subject matcher. Special cases per the spec:
  //   - pattern "WEBServer"  matches "WEBServer" and "WEBServer:<anything>"
  //   - pattern "WEBServer:" matches "WEBServer" only (empty principal)
  //   - pattern "WEBServer:*" matches "WEBServer:<non-empty>" only
  //   - otherwise: plain glob match.
  static bool subject_matches(const std::string& pattern, const std::string& subject) {
    if (pattern == subject) return true;
    if (glob(pattern, subject)) return true;
    // Bare-module pattern: allow any principal extension.
    if (pattern.find(':') == std::string::npos && pattern.find('*') == std::string::npos && subject.size() > pattern.size() && subject[pattern.size()] == ':' &&
        boost::algorithm::to_lower_copy(subject.substr(0, pattern.size())) == boost::algorithm::to_lower_copy(pattern)) {
      return true;
    }
    // Trailing-colon pattern means "empty principal only". make_subject()
    // collapses an empty principal to just the module name (no colon), so
    // "WEBServer:" must also match a bare "WEBServer" subject.
    if (!pattern.empty() && pattern.back() == ':' && pattern.find('*') == std::string::npos && subject.find(':') == std::string::npos &&
        boost::algorithm::to_lower_copy(pattern.substr(0, pattern.size() - 1)) == boost::algorithm::to_lower_copy(subject)) {
      return true;
    }
    return false;
  }

  // Object matcher. Per the spec, a bare command (no dot) on the pattern
  // side matches any target module. So "check_cpu" matches
  // "CheckSystem.check_cpu" as well as "CheckHelpers.check_cpu".
  static bool object_matches(const std::string& pattern, const std::string& object) {
    if (glob(pattern, object)) return true;
    if (pattern.find('.') == std::string::npos) {
      // Compare the pattern against the command portion only.
      const std::size_t dot = object.rfind('.');
      if (dot != std::string::npos) {
        return glob(pattern, object.substr(dot + 1));
      }
    }
    return false;
  }

  mutable std::mutex mutex_;
  bool enabled_;
  bool allow_exec_;
  bool log_denials_;
  bool log_allows_;
  std::vector<rule> rules_;
};

}  // namespace core
}  // namespace nsclient
