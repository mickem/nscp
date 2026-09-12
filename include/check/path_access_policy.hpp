// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <check/access_policy.hpp>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// The path-shaped variant of check::access::policy, for arguments which name a
// file (`check_logfile file=`).
//
// A path cannot be matched as a plain string. `C:/logs/../../Windows/x.log`
// and a symlink planted inside an allowed directory both read as "under
// C:/logs" to a glob and are not, so every candidate is resolved with
// weakly_canonical first - which flattens `..` and follows symlinks, including
// Windows junctions - and the *resolved* path is what gets matched and what
// the check then opens. Matching one path and opening another is the bug this
// exists to prevent, so in a restricted mode the check reads the resolved path
// even when that spells the name differently than the caller did.
//
// Allow-list entries come in two shapes, distinguished by whether they contain
// a wildcard:
//
//   /var/log            a directory: everything beneath it, at any depth
//   /var/log/*.log      a glob: matched against the whole resolved path
//
// A bare entry which names an existing regular file is that one file. A bare
// entry which names nothing yet is treated as a directory: the volume may be
// mounted, or the application may create its log directory, after the
// service has started, and an entry which is silently demoted to an exact
// file name at load time would then refuse everything beneath it until the
// next reload. Treating it as a directory costs nothing - a directory entry
// covers the path itself too - and matches what the documentation promises.
//
// In a path glob `*` and `?` do not cross a directory separator, so
// `/var/log/*.log` names the files in that directory and not those in
// `/var/log/private/`; `**` matches across separators for the cases where
// the whole subtree is meant (`/var/log/**.log`).

namespace check {
namespace access {

class path_policy {
 public:
  path_policy(std::string noun, std::string nouns, std::string settings_path)
      : base_(std::move(noun), std::move(nouns), std::move(settings_path),
// Path comparison follows the platform: case-insensitive where the
// filesystem is, case-sensitive where /var/log/App.log and
// /var/log/app.log are two different files and an allow list must not
// conflate them.
#ifdef WIN32
              false
#else
              true
#endif
        ) {}

  path_policy(const path_policy &other) : base_(other.base_) {
    std::lock_guard<std::mutex> lock(other.mutex_);
    entries_ = other.entries_;
  }
  path_policy &operator=(const path_policy &other) {
    if (this == &other) return *this;
    path_policy copy(other);
    base_ = copy.base_;
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.swap(copy.entries_);
    return *this;
  }

  // --- configuration -------------------------------------------------------

  void set_mode(const std::string &value) { base_.set_mode(value); }
  void add_predefined(const std::string &name, const std::string &value) { base_.add_predefined(name, value); }
  void clear_predefined() { base_.clear_predefined(); }

  // Built beside the live list and swapped in whole, for the same reason as
  // policy::set_allow_list.
  void set_allow_list(const std::string &value) {
    std::vector<entry> entries;
    std::vector<std::string> raw;
    boost::algorithm::split(raw, value, boost::algorithm::is_any_of(","));
    for (std::string &item : raw) {
      boost::algorithm::trim(item);
      if (item.empty()) continue;
      entries.push_back(make_entry(item));
    }
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.swap(entries);
  }

  // Only the predefined entries are appended to by the settings callbacks;
  // the mode and the allow list are replaced by theirs, and clearing them
  // here would open the gate until notify() has run (see policy::reset).
  void reset() { base_.reset(); }

  // --- state ---------------------------------------------------------------

  mode get_mode() const { return base_.get_mode(); }
  bool is_restricted() const { return base_.is_restricted(); }
  std::string get_config_error() const { return base_.get_config_error(); }
  std::size_t allow_list_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
  }

  // Normalise a path the way resolve() does, so a caller can match a candidate
  // without going through the mode machinery (used by the tests).
  static std::string canonical(const std::string &path) {
    boost::system::error_code ec;
    const boost::filesystem::path resolved = boost::filesystem::weakly_canonical(boost::filesystem::path(path), ec);
    // weakly_canonical needs the filesystem to resolve symlinks; when it
    // cannot (a path on a volume which is gone, a permission error on a parent
    // directory) fall back to a purely lexical normalisation. That still
    // flattens `..`, so the traversal case stays closed; only symlink
    // resolution is lost, and an entry which is not reachable cannot be read
    // by the check either.
    const boost::filesystem::path out = ec ? boost::filesystem::path(path).lexically_normal() : resolved;
    return to_slashes(out.string());
  }

  // Translate a path glob into a regular expression. Unlike the generic
  // glob_to_regex, `*` and `?` stop at a directory separator (the candidate
  // is slash-normalised by canonical(), so `/` is the only one to consider)
  // and `**` is the spelling which crosses them.
  static std::string glob_to_regex(const std::string &glob) {
    std::string re;
    re.reserve(glob.size() * 2);
    for (std::string::size_type i = 0; i < glob.size(); ++i) {
      const char c = glob[i];
      switch (c) {
        case '*':
          if (i + 1 < glob.size() && glob[i + 1] == '*') {
            re += ".*";
            ++i;
          } else {
            re += "[^/]*";
          }
          break;
        case '?':
          re += "[^/]";
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

  bool matches_allow_list(const std::string &resolved_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return matches_allow_list_unlocked(resolved_path);
  }

  // --- resolution ----------------------------------------------------------

  // Resolve one caller-supplied file argument. On success `value` holds the
  // path the check should open: the token untouched when nothing is
  // restricted, the configured path when the token named a predefined entry,
  // and the resolved path when it had to pass the allow list.
  decision resolve(const std::string &token) const {
    std::string configured;
    if (base_.lookup_predefined(token, configured)) return decision::accept(configured);

    const std::string config_error = base_.get_config_error();
    if (!config_error.empty()) return decision::refuse(config_error);

    switch (base_.get_mode()) {
      case mode::any:
        return decision::accept(token);
      case mode::allowed: {
        const std::string resolved = canonical(token);
        if (matches_allow_list(resolved)) return decision::accept(resolved);
        return decision::refuse("Refusing file '" + token + "': it is not in 'allowed files' (see [" + base_.get_settings_path() + "] in the configuration)");
      }
      case mode::predefined:
      default:
        return decision::refuse("Refusing file '" + token + "': 'file access' is set to predefined, so only a configured file name may be used (see [" +
                                base_.get_settings_path() + "] in the configuration)");
    }
  }

 private:
  struct entry {
    bool is_directory;
    std::string text;      // directory entries: the resolved directory
    boost::regex pattern;  // glob entries
    entry() : is_directory(false) {}
  };

  bool matches_allow_list_unlocked(const std::string &resolved_path) const {
    for (const entry &e : entries_) {
      if (e.is_directory) {
        if (is_under(e.text, resolved_path)) return true;
      } else if (boost::regex_match(resolved_path, e.pattern)) {
        return true;
      }
    }
    return false;
  }

  static std::string to_slashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    // A trailing separator would make the containment test compare an empty
    // final element; drop it so "/var/log/" and "/var/log" behave alike.
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
  }

  static bool has_wildcard(const std::string &s) { return s.find('*') != std::string::npos || s.find('?') != std::string::npos; }

  static entry make_entry(const std::string &item) {
    entry e;
    if (!has_wildcard(item)) {
      // Everything which is not an existing regular file is a directory
      // entry: an existing directory obviously, and a path which does not
      // exist yet (see the header comment). Only a file which is there now is
      // pinned to that one file.
      boost::system::error_code ec;
      const boost::filesystem::file_status status = boost::filesystem::status(boost::filesystem::path(item), ec);
      const bool is_file = !ec && boost::filesystem::exists(status) && !boost::filesystem::is_directory(status);
      e.is_directory = !is_file;
      e.text = canonical(item);
      if (e.is_directory) return e;
    } else {
      // Resolve the literal directory prefix of the glob, so a pattern written
      // through a symlinked or `..`-bearing path still matches the resolved
      // candidates it is meant to cover. Everything from the first wildcard on
      // is left alone - it is a pattern, not a path.
      e.text = resolve_glob_prefix(item);
    }
    boost::regex::flag_type flags = boost::regex::perl;
#ifdef WIN32
    flags |= boost::regex::icase;
#endif
    e.pattern = boost::regex(glob_to_regex(e.text), flags);
    return e;
  }

  static std::string resolve_glob_prefix(const std::string &item) {
    const std::string normalised = to_slashes(item);
    const std::string::size_type wild = normalised.find_first_of("*?");
    const std::string::size_type slash = normalised.rfind('/', wild);
    if (slash == std::string::npos || slash == 0) return normalised;
    const std::string prefix = normalised.substr(0, slash);
    const std::string rest = normalised.substr(slash);
    if (has_wildcard(prefix)) return normalised;
    return canonical(prefix) + rest;
  }

  // Compare two path fragments the way the platform's filesystem does: the
  // glob entries are compiled case-insensitively on Windows, and a directory
  // entry has to agree with them or the same tree is allowed under one
  // spelling and refused under another. weakly_canonical keeps whatever
  // case the caller wrote for the elements it resolves lexically, so this
  // cannot be left to it.
  static bool same_text(const std::string &a, const std::string &b) {
#ifdef WIN32
    return boost::algorithm::iequals(a, b);
#else
    return a == b;
#endif
  }

  // True when `file` sits inside `dir` (or is `dir` itself). Both are already
  // resolved and slash-normalised, so this is a plain element-wise prefix
  // test: comparing the strings would let "/var/logger/x" pass for "/var/log".
  static bool is_under(const std::string &dir, const std::string &file) {
    if (same_text(file, dir)) return true;
    if (file.size() <= dir.size()) return false;
    if (!same_text(file.substr(0, dir.size()), dir)) return false;
    return file[dir.size()] == '/';
  }

  policy base_;
  mutable std::mutex mutex_;
  std::vector<entry> entries_;
};

}  // namespace access
}  // namespace check
