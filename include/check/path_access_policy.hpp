// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <check/access_policy.hpp>
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
// A bare entry which is not a directory is an exact file name.

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
      entries_.push_back(make_entry(item));
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

  bool matches_allow_list(const std::string &resolved_path) const {
    for (const entry &e : entries_) {
      if (e.is_directory) {
        if (is_under(e.text, resolved_path)) return true;
      } else if (boost::regex_match(resolved_path, e.pattern)) {
        return true;
      }
    }
    return false;
  }

  // --- resolution ----------------------------------------------------------

  // Resolve one caller-supplied file argument. On success `value` holds the
  // path the check should open: the token untouched when nothing is
  // restricted, the configured path when the token named a predefined entry,
  // and the resolved path when it had to pass the allow list.
  decision resolve(const std::string &token) const {
    std::string configured;
    if (base_.lookup_predefined(token, configured)) return decision::accept(configured);

    if (!base_.get_config_error().empty()) return decision::refuse(base_.get_config_error());

    switch (base_.get_mode()) {
      case mode::any:
        return decision::accept(token);
      case mode::allowed: {
        const std::string resolved = canonical(token);
        if (matches_allow_list(resolved)) return decision::accept(resolved);
        return decision::refuse("Refusing file '" + token + "': it is not in 'allowed files' (see [" + base_.get_settings_path() +
                                "] in the configuration)");
      }
      case mode::predefined:
      default:
        return decision::refuse("Refusing file '" + token +
                                "': 'file access' is set to predefined, so only a configured file name may be used (see [" + base_.get_settings_path() +
                                "] in the configuration)");
    }
  }

 private:
  struct entry {
    bool is_directory;
    std::string text;      // directory entries: the resolved directory
    boost::regex pattern;  // glob entries
    entry() : is_directory(false) {}
  };

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
      boost::system::error_code ec;
      const bool dir = boost::filesystem::is_directory(boost::filesystem::path(item), ec);
      e.is_directory = !ec && dir;
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

  // True when `file` sits inside `dir` (or is `dir` itself). Both are already
  // resolved and slash-normalised, so this is a plain element-wise prefix
  // test: comparing the strings would let "/var/logger/x" pass for "/var/log".
  static bool is_under(const std::string &dir, const std::string &file) {
    if (file == dir) return true;
    if (file.size() <= dir.size()) return false;
    if (file.compare(0, dir.size(), dir) != 0) return false;
    return file[dir.size()] == '/';
  }

  policy base_;
  std::vector<entry> entries_;
};

}  // namespace access
}  // namespace check
