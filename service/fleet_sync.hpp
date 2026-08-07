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
#include <nscapi/protobuf/metrics.hpp>
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
  bool metrics = true;
  // Deadline for a single read/write against the fleet server. A server that
  // accepts the connection and then stops responding would otherwise block the
  // sync thread for good - the host would stay enrolled but stop being managed.
  unsigned int timeout_seconds = 60;
};

// The post-enrollment fleet sync loop (see the fleet agent integration
// reference), running inside the core service: polls desired state over mTLS,
// downloads + verifies bundles, applies them all-or-nothing (merged JSON
// config rendered to <managed_path>/fleet.ini, scripts staged under
// <managed_path>/scripts), reports state and metrics, and renews the client
// certificate before expiry. The core starts it at boot only when the
// enrollment manifest exists (see fleet_sync::has_manifest) and stops it at
// shutdown; it survives configuration reloads, which it itself requests
// (via `request_reload`) after applying new configuration.
class fleet_sync {
 public:
  typedef std::function<void()> reload_function;

  // True when the enrollment manifest (agent-state.json) exists - the boot
  // gate for starting the sync thread at all.
  static bool has_manifest(const std::string &state_file);

  fleet_sync(nsclient::logging::logger_instance logger, fleet_config config, nsclient::core::tag_repository_instance tags, reload_function request_reload);
  ~fleet_sync();
  void stop();

  // Take a snapshot of the core metrics set (called from the scheduler thread
  // every `metrics interval`): the samples are flattened, kept and submitted
  // by the next poll cycle. Only the latest snapshot is retained - the fleet
  // server samples at poll resolution, so buffering older values would only
  // grow the payload.
  void on_metrics(const PB::Metrics::MetricsMessage &metrics);

 private:
  void thread_proc();
  // One poll cycle; returns how many seconds to sleep before the next one.
  unsigned long poll_once();
  // `stale` is set when the server says a bundle is no longer ours (the
  // desired state changed under us): the cycle is abandoned without reporting
  // a failure and the next poll picks up the new state.
  bool apply_state(const onboarding::desired_state &state, std::vector<std::string> &errors, bool &stale);
  bool fetch_bundle(const onboarding::bundle_info &bundle, std::string &bytes, std::string &error, bool &gone);

  http::response do_call(const char *verb, const std::string &path, const std::string &payload = "");
  void report_state(const boost::optional<std::string> &applied_hash, const std::vector<std::string> &errors);
  void post_metrics();
  void maybe_renew();
  std::map<std::string, std::string> collect_tags() const;

  // Connection-failure bookkeeping: log a classified, actionable error the
  // first time a failure (or a new kind of failure) appears, demote repeats
  // to debug with a periodic reminder, and announce recovery.
  void log_transport_failure(const std::string &operation, const std::string &message);
  void note_transport_success();

  void load_applied_state();
  void save_applied_state() const;

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
  std::vector<onboarding::installed_bundle> installed_;
  unsigned long poll_interval_ = 60;
  unsigned int failures_ = 0;
  std::chrono::steady_clock::time_point started_;

  // The latest metrics snapshot, produced on the scheduler thread and consumed
  // by the poll loop.
  mutable boost::mutex metrics_mutex_;
  std::vector<onboarding::metric_sample> metrics_;
  bool metrics_truncated_logged_ = false;

  // Transport state shared by all calls in the loop: once the server is known
  // to be unreachable, secondary traffic (renewal, metrics) is skipped until a
  // call succeeds again, and repeated identical errors stay out of the error log.
  bool transport_ok_ = true;
  std::string last_transport_error_;
  unsigned long transport_failures_ = 0;

  std::shared_ptr<boost::thread> thread_;
};
