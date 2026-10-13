// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <net/auth_rate_limiter.hpp>
#include <net/connection_log_throttle.hpp>
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

// The token rate limiter and the locks that make its check-compare-count one
// step. Owned by the module and handed to each controller, so a settings
// reload - which builds a new controller - does not hand a blocked guesser a
// fresh set of tries or reset its backoff.
struct ncpa_auth_state {
  // Guards the limiter's check-and-count and the controller's digest cache.
  // Held only across in-memory work, never across a token derivation.
  std::mutex mutex;
  auth_rate_limiter limiter;
  // Verifying a hashed token is a PBKDF2 derivation, tens of milliseconds, and
  // it must not be done under `mutex`: every request on every worker would
  // queue behind it, and a host sending wrong tokens could stall the poller.
  // It still has to be one step per *address* - with several workers,
  // parallel guesses from one host would otherwise all pass is_blocked()
  // before any of them was counted. So an address holds one of these for the
  // check, the derivation and the count, and other addresses go on. Striped
  // rather than one per address so the table cannot grow; two addresses that
  // share a stripe wait for each other, which is rare and harmless.
  static constexpr std::size_t kVerifyStripes = 64;
  std::array<std::mutex, kVerifyStripes> verify_stripes;
  std::mutex &verify_lock_for(const std::string &remote) { return verify_stripes[std::hash<std::string>()(remote) % kVerifyStripes]; }
  // Any host in `allowed hosts` can make the listener refuse a request, so
  // refusals are logged through this: one line per address per minute,
  // counting the ones left out. Locks for itself.
  net::connection_log_throttle refusals;
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
  // The tokens accepted so far, so a poller pays the derivation once rather
  // than on every request. Guarded by auth_->mutex. Per controller, as the
  // tokens are: a reload with new tokens starts with an empty cache.
  ncpa::accepted_tokens accepted_;
};
