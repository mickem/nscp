// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <Request.h>
#include <StreamResponse.h>

#include <boost/thread/mutex.hpp>
#include <memory>
#include <net/http/auth_rate_limiter.hpp>
#include <net/socket/allowed_hosts.hpp>
#include <string>
#include <vector>

#include "ncpa_sources.hpp"
#include "ncpa_tree.hpp"

// The HTTP half of the NCPA bridge: one controller on `/api` that authenticates
// the NCPA token, resolves the path against the node tree and answers either
// the subtree as JSON or a check result.
namespace ncpa {

// Everything the controller needs that a settings reload can change, plus the
// per-peer auth state. Owned by NCPAServer and shared with the controller, so a
// reload rewrites the configuration in place rather than restarting the
// listener under an in-flight request.
class session {
 public:
  // Whether `candidate` is the configured token or the backup one. Compared in
  // constant time: a length-or-prefix comparison leaks the token one byte at a
  // time to anyone who can measure a few thousand requests.
  bool token_matches(const std::string &candidate) const;
  // False when no token is configured at all, which is what makes the server
  // refuse every request until an operator sets one.
  bool has_token() const;
  void set_token(const std::string &token);
  void set_backup_token(const std::string &token);

  void set_allowed_hosts(const std::string &hosts);
  void set_allowed_hosts_cache(bool cached);
  // `errors` collects why an address was refused, for the log line.
  bool is_allowed(const std::string &ip, std::list<std::string> &errors) const;
  std::list<std::string> boot();

  void set_allow_arguments(bool allow);
  bool allow_arguments() const;

  // `any`, `scripts`, or a comma-separated list of query names.
  void set_exposed_plugins(const std::string &spec);
  // Whether `name` may be called through `plugins/`.
  bool exposes(const std::string &name) const;
  // Whether every registered query is exposed, which decides whether the bare
  // `/api/plugins` listing asks the core for an inventory at all.
  bool exposes_everything() const;
  // Whether `plugins = scripts`. Which queries that means is a question only
  // the core can answer (it is the ones CheckExternalScripts registered), so
  // the dispatcher resolves it rather than this class.
  bool scripts_only() const;
  std::vector<std::string> plugin_allow_list() const;

  void set_default_units(const std::string &units);
  std::string default_units() const;

  void set_expose_version(bool expose);
  bool expose_version() const;

  auth_rate_limiter &rate_limiter() { return rate_limiter_; }

 private:
  mutable boost::mutex mutex_;
  std::string token_;
  std::string backup_token_;
  // Mutable because a lookup refreshes the resolved list when caching is off,
  // which is a cache update rather than a change of configuration.
  mutable socket_helpers::allowed_hosts_manager allowed_hosts_;
  bool allow_arguments_ = false;
  // Empty means "any": every registered query is callable by name.
  std::vector<std::string> exposed_;
  bool scripts_only_ = false;
  std::string default_units_;
  bool expose_version_ = true;
  auth_rate_limiter rate_limiter_;
};

typedef std::shared_ptr<session> session_ptr;

// Percent-decode one path segment. check_ncpa.py quotes every `-a` argument
// with `safe=''`, so `warning=used>80%` arrives as `warning%3Dused%3E80%25` and
// nothing downstream would recognise it undecoded.
std::string url_decode(const std::string &value);

class controller : public Mongoose::RegexpController {
 public:
  controller(session_ptr session, metrics_snapshot *snapshot, query_dispatcher *dispatcher, delta_store *deltas, tree_options options);

  void handle_root(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void handle_path(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);

 private:
  void serve(const std::string &accessor, Mongoose::Request &request, Mongoose::StreamResponse &response);
  // Authenticate and rate-limit. Writes the refusal into `response` and returns
  // false when the request must not be served.
  bool authenticate(Mongoose::Request &request, Mongoose::StreamResponse &response) const;
  // Read one parameter from the query string, falling back to a form-encoded
  // POST body - the NCPA agent accepts either.
  static std::string parameter(const Mongoose::Request &request, const std::string &key, const std::string &fallback = "");
  static bool has_parameter(const Mongoose::Request &request, const std::string &key);
  // NCPA's truthiness for `check` and `delta`: the parameter counts as set when
  // it is present and not obviously negated.
  static bool flag(const Mongoose::Request &request, const std::string &key);

  session_ptr session_;
  metrics_snapshot *snapshot_;
  query_dispatcher *dispatcher_;
  delta_store *deltas_;
  tree_options options_;
};

}  // namespace ncpa
