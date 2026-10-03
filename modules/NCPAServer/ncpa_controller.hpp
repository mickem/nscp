// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>

#include <memory>
#include <mutex>
#include <net/auth_rate_limiter.hpp>
#include <string>
#include <vector>

#include "ncpa_protocol.hpp"
#include "ncpa_sources.hpp"

// Everything a request is judged against. Captured when the listener starts
// and never changed under it: a settings reload stops the listener and starts
// a new one with a new controller, so nothing here needs a lock.
struct ncpa_config {
  std::string token;
  std::string backup_token;
  bool allow_arguments = false;
  ncpa::plugin_policy plugins;
  int auth_max_failures = auth_rate_limiter::kDefaultMaxFailures;
  int auth_block_seconds = auth_rate_limiter::kDefaultBlockSeconds;
};

// The token rate limiter and the lock that makes its check-compare-count one
// step. Owned by the module and handed to each controller, so a settings
// reload - which builds a new controller - does not hand a blocked guesser a
// fresh set of tries or reset its backoff.
struct ncpa_auth_state {
  std::mutex mutex;
  auth_rate_limiter limiter;
};

// The one controller on /api. It authenticates the request, resolves the node
// path and answers in the NCPA JSON shapes; the `plugins/` node hands the
// request to the core as an ordinary query. Called from several worker
// threads at once (setWorkerThreads), so everything it holds is either
// immutable or locks for itself.
class ncpa_controller : public Mongoose::RegexpController {
 public:
  ncpa_controller(ncpa_config config, std::shared_ptr<ncpa_sources> sources, std::shared_ptr<ncpa_auth_state> auth);

  void api(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);

 private:
  void handle(Mongoose::Request &request, const boost::smatch &what, Mongoose::StreamResponse &response);
  // Whether the request may go on. On false the response has been written.
  bool authenticate(const Mongoose::Request &request, const ncpa::form_vector &args, Mongoose::StreamResponse &response);

  // /api/plugins and /api/plugins/<name>/<args...>
  void plugins(const std::vector<std::string> &path, const ncpa::form_vector &args, bool check_mode, const std::string &full_path, const std::string &remote,
               Mongoose::StreamResponse &response);
  // The names `plugins/` lists, sorted.
  std::vector<std::string> exposed_plugins() const;

  const ncpa_config config_;
  std::shared_ptr<ncpa_sources> sources_;
  // Shared with the module; see ncpa_auth_state.
  std::shared_ptr<ncpa_auth_state> auth_;
};
