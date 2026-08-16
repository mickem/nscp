// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <net/http/http_response.hpp>
#include <nsclient/logger/logger.hpp>
#include <onboarding/onboarding.hpp>
#include <onboarding/sync.hpp>
#include <string>
#include <vector>

#include "tag_repository.hpp"

struct fleet_config {
  std::string state_file;    // expanded path to the enrollment manifest (agent-state.json)
  std::string managed_path;  // expanded directory for fleet.ini, scripts and the bundle cache
  std::string hostname;      // hostname reported as a tag (already expanded)
  std::string tls_version;
  std::string nscp_version;  // reported as a tag
  // Deadline for a single read/write against the fleet server. A server that
  // accepts the connection and then stops responding would otherwise block the
  // sync thread for good - the host would stay enrolled but stop being managed.
  unsigned int timeout_seconds = 60;
};

// The post-enrollment fleet sync loop (see the fleet agent integration
// reference), running inside the core service: polls desired state over mTLS,
// downloads + verifies bundles, applies them all-or-nothing (merged JSON
// config rendered to <managed_path>/fleet.ini, scripts staged under
// <managed_path>/scripts), reports state, and renews the client
// certificate before expiry. The core starts it at boot only when the
// enrollment manifest is readable (see fleet_sync::check_manifest) and stops it at
// shutdown; it survives configuration reloads, which it itself requests
// (via `request_reload`) after applying new configuration.
class fleet_sync {
 public:
  typedef std::function<void()> reload_function;

  // Why the sync will or will not start, as decided at boot.
  //
  // `unreadable` is deliberately distinct from `missing`: a host that was never
  // enrolled is the normal case and stays quiet, while a manifest that exists
  // but cannot be opened is a misconfiguration that has to be reported. That
  // case is what an enrollment run under sudo produces on a packaged install,
  // where the service runs as an unprivileged account - and testing only for
  // existence made it start a sync that then died on the first read, leaving an
  // agent that looks healthy and never joins the fleet.
  enum class manifest_status { missing, unreadable, present };

  // `detail` is filled for `unreadable` with an operator-actionable
  // description (who owns the file, who we are running as).
  static manifest_status check_manifest(const std::string &state_file, std::string &detail);

  fleet_sync(nsclient::logging::logger_instance logger, fleet_config config, nsclient::core::tag_repository_instance tags, reload_function request_reload);
  ~fleet_sync();
  void stop();

 private:
  // Retry wrapper: catches whatever escapes run() and starts it again after a
  // widening delay, so no single failure ends the sync for the life of the
  // process.
  void thread_proc();
  // One life of the sync: load the identity, then poll until interrupted.
  // Returns normally only for a deliberate stop; everything else throws and is
  // retried by thread_proc.
  void run();
  // One poll cycle; returns how many seconds to sleep before the next one.
  unsigned long poll_once();
  // `stale` is set when the server says a bundle is no longer ours (the
  // desired state changed under us): the cycle is abandoned without reporting
  // a failure and the next poll picks up the new state.
  // `reload_needed` is set true only when the applied content (rendered
  // fleet.ini + staged scripts) actually changed: a state that renders no
  // different output is recorded but does not trigger a service reload.
  bool apply_state(const onboarding::desired_state &state, std::vector<std::string> &errors, bool &stale, bool &reload_needed);
  bool fetch_bundle(const onboarding::bundle_info &bundle, std::string &bytes, std::string &error, bool &gone);

  // max_response_bytes overrides the HTTP client's default body cap for this
  // call (0 keeps the default, which is generous for JSON but far below a
  // bundle): bundle downloads pass the larger max_bundle_script_bytes.
  http::response do_call(const char *verb, const std::string &path, const std::string &payload = "", std::size_t max_response_bytes = 0);
  void report_state(const boost::optional<std::string> &applied_hash, const std::vector<std::string> &errors);
  void maybe_renew();
  std::map<std::string, std::string> collect_tags() const;

  // Connection-failure bookkeeping: log a classified, actionable error the
  // first time a failure (or a new kind of failure) appears, demote repeats
  // to debug with a periodic reminder, and announce recovery.
  void log_transport_failure(const std::string &operation, const std::string &message);
  void note_transport_success();

  void load_applied_state();
  void save_applied_state() const;
  // On startup, roll back a scripts.old left by an apply that died mid-swap and
  // force a re-fetch, so the agent never reports in sync with an incomplete tree.
  void recover_interrupted_apply();

  void log_error(const std::string &message) const;
  void log_info(const std::string &message) const;
  void log_debug(const std::string &message) const;

  nsclient::logging::logger_instance logger_;
  fleet_config config_;
  // The core's central tag repository: module-contributed key=value facts
  // (drives=c:,d:, sqlserver=detected, ...) merged into every state report's
  // reported_tags. The revision drives change detection - whenever it moves,
  // the loop sends a fresh state report so the fleet server re-evaluates
  // group membership promptly.
  nsclient::core::tag_repository_instance tags_;
  unsigned long long reported_tag_revision_ = 0;
  bool tags_reported_ = false;
  reload_function request_reload_;

  onboarding::enrolled_identity identity_;
  std::string current_hash_;
  // Hash of the applied content (rendered fleet.ini + staged script tree),
  // persisted alongside current_hash_. Lets a state whose bundles changed but
  // whose rendered output did not skip an otherwise pointless service reload.
  std::string content_hash_;
  std::vector<onboarding::installed_bundle> installed_;
  unsigned long poll_interval_ = 60;
  unsigned int failures_ = 0;

  // Transport state shared by all calls in the loop: once the server is known
  // to be unreachable, secondary traffic (renewal) is skipped until a
  // call succeeds again, and repeated identical errors stay out of the error log.
  bool transport_ok_ = true;
  std::string last_transport_error_;
  unsigned long transport_failures_ = 0;

  std::shared_ptr<boost::thread> thread_;
};
