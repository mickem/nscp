// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <net/http/http_response.hpp>
#include <nsclient/logger/logger.hpp>
#include <onboarding/facts_pacer.hpp>
#include <onboarding/failure_context_pacer.hpp>
#include <onboarding/onboarding.hpp>
#include <onboarding/sync.hpp>
#include <string>
#include <vector>

#include "fact_repository.hpp"
#include "failed_check_store.hpp"
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
  // Answers "does this host carry local configuration that outranks what we
  // send it?" for the state report. A callback rather than a captured bool
  // because the sync outlives configuration reloads - including the ones it
  // triggers itself - and the answer can change under it.
  std::function<bool()> local_config_probe;
  // Allow a management url (`mtls_url`) that is not https. Off by default: the
  // client certificate and the pinned server certificate protect nothing on a
  // plain socket, so a plaintext url leaves desired state and signed-bundle
  // delivery - remote code execution - unauthenticated. Comes from the same
  // `[tls] allow plaintext` boot.ini key that allows a plaintext settings
  // source, which is the same question about the same agent.
  bool allow_plaintext = false;
  // [/settings/fleet] failure context: report a failed check, with the
  // metrics snapshot it was recorded with, to a server that asks for it - at
  // most one report per `failure_context_interval` seconds.
  bool failure_context = true;
  unsigned long failure_context_interval = onboarding::failure_context_pacer::default_interval_seconds;
  // [/settings/fleet] poll interval, min poll interval, max poll interval: how
  // often the host asks the fleet server for its desired state. The server
  // names the interval in every answer; `poll_interval` is only what is used
  // until it has, and the other two bound what it may ask for. They bound the
  // interval alone: a Retry-After is a wait the server named for one call and
  // is slept as given, and the backoff after a failed poll is a multiple of
  // the interval in force.
  unsigned long poll_interval = 60;
  unsigned long min_poll_interval = 1;
  unsigned long max_poll_interval = 86400;
};

// What the sync reaches the outside world through: the fleet server and the
// two clocks. The defaults (everything empty) are the real ones. They are
// replaceable so a unit test can answer a call with any status, and move time
// by a minute, without a server, a socket or a wait - see fleet_sync_test.cpp.
// The pacing rules themselves are tested where they live
// (onboarding::facts_upload_pacer, onboarding::failure_context_pacer); this is
// for what the sync makes of an answer.
struct fleet_seams {
  // One call to the fleet server; `path` is relative to the management url.
  // Throws for a call that got no answer, as the http client does.
  typedef std::function<http::response(const char *verb, const std::string &path, const std::string &payload, std::size_t max_response_bytes)>
      transport_function;
  transport_function transport;
  // Seconds since the epoch, for what is persisted across restarts.
  std::function<std::int64_t()> wall_clock;
  std::function<std::chrono::steady_clock::time_point()> steady_clock;
  // Off for a sync that is stepped by hand rather than run on its thread.
  bool start_thread = true;
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

  fleet_sync(nsclient::logging::logger_instance logger, fleet_config config, nsclient::core::tag_repository_instance tags,
             nsclient::core::fact_repository_instance facts, nsclient::core::failed_check_store_instance failures, reload_function request_reload,
             fleet_seams seams = fleet_seams());
  ~fleet_sync();
  void stop();

 private:
  // Steps a sync built without its thread: begin(), then turn() by turn().
  friend struct fleet_sync_test_access;

  // Retry wrapper: catches whatever escapes run() and starts it again after a
  // widening delay, so no single failure ends the sync for the life of the
  // process.
  void thread_proc();
  // One life of the sync: load the identity, then poll until interrupted.
  // Returns normally only for a deliberate stop; everything else throws and is
  // retried by thread_proc.
  void run();
  // The start of a life: the identity, the applied state, the heartbeat and
  // the first state report. False for a deliberate stop (not enrolled, a
  // plaintext url nobody allowed, a revoked certificate).
  bool begin();
  // One poll cycle; returns how many seconds to sleep before the next one.
  // How long to sleep before the next poll, and whether that is a wait the
  // server named (a Retry-After): a minimum, jittered upwards only.
  struct poll_sleep {
    unsigned long seconds;
    bool at_least;
    poll_sleep(const unsigned long seconds_, const bool at_least_ = false) : seconds(seconds_), at_least(at_least_) {}  // NOLINT: implicit on purpose
  };
  poll_sleep poll_once();
  // One turn of the loop - the poll and everything that follows it - returning
  // the sleep before the next.
  poll_sleep turn();
  // `stale` is set when the server says a bundle is no longer ours (the
  // desired state changed under us): the cycle is abandoned without reporting
  // a failure and the next poll picks up the new state.
  // `reload_needed` is set true only when the applied content (rendered
  // fleet.ini + staged scripts) actually changed: a state that renders no
  // different output is recorded but does not trigger a service reload.
  bool apply_state(const onboarding::desired_state &state, std::vector<std::string> &errors, bool &stale, bool &reload_needed);
  bool fetch_bundle(const onboarding::bundle_info &bundle, const onboarding::bundle_descriptor &descriptor, std::string &bytes, std::string &error, bool &gone);

  // max_response_bytes overrides the HTTP client's default body cap for this
  // call (0 keeps the default, which is generous for JSON but far below a
  // bundle): bundle downloads pass the larger max_bundle_script_bytes.
  http::response do_call(const char *verb, const std::string &path, const std::string &payload = "", std::size_t max_response_bytes = 0);
  void report_state(const boost::optional<std::string> &applied_hash, const std::vector<std::string> &errors);
  void maybe_renew();
  // An interval the server asked for, held to the operator's bounds.
  unsigned long bounded_poll_interval(unsigned long seconds) const;
  std::map<std::string, std::string> collect_tags() const;

  // Upload the facts document to /agent/v1/facts when the server has said it
  // holds a different one. Cheap when it has not: one hash compare.
  void maybe_upload_facts();
  // Report the newest failed check, with its metrics snapshot, to
  // /agent/v1/failure-context when the server takes reports and the interval
  // since the last one has passed. Cheap otherwise: one index compare.
  void maybe_report_failure_context();
  // Which fleet call a response answered: they share one rule, which differs
  // only in what trouble and a Retry-After mean for the facts upload and the
  // failure report.
  enum class fleet_call { poll, report, upload, failure_report };
  void note_server_response(const http::response &response, fleet_call call);
  // The X-Facts-Hash on a response the server meant, for both readers - what
  // it holds after a poll or report, what it kept after an upload. None when
  // there is no header, or one that cannot be read (logged once).
  // `call` only picks the wording of the log line.
  boost::optional<std::string> read_facts_hash_header(const http::response &response, fleet_call call);
  void log_facts_failure(const std::string &hash, unsigned int status, const std::string &message);
  // The current facts hash for the state report; empty without a repository.
  std::string current_facts_hash() const;

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

  std::int64_t wall_now() const;
  std::chrono::steady_clock::time_point steady_now() const;

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
  // The core's facts repository: the host inventory, uploaded whole on its
  // own call whenever its hash differs from what the server holds, while
  // every state report carries only the hash.
  nsclient::core::fact_repository_instance facts_;
  // When to upload the facts document: only on a miss the server reported,
  // paced per document. See onboarding::facts_upload_pacer for the rules.
  onboarding::facts_upload_pacer facts_pacer_;
  // Said once until the condition clears: an unreadable X-Facts-Hash, and a
  // facts hash that could not be computed.
  bool bad_facts_header_logged_ = false;
  mutable bool facts_hash_failure_logged_ = false;
  // The last failed upload that was logged, so a failure repeated at every
  // backoff step is logged once per document and status - not per body,
  // which may carry a request id that differs every time.
  // The loss episodes the "lost" and "keeps losing" lines were last said for.
  unsigned long lost_logged_episode_ = 0;
  unsigned long hourly_logged_episode_ = 0;
  std::string last_facts_error_hash_;
  unsigned int last_facts_error_status_ = 0;
  std::chrono::steady_clock::time_point last_facts_error_at_;
  // The core's failed-check log, which the failure report is drawn from.
  nsclient::core::failed_check_store_instance failed_checks_;
  // When to report a failure: one per interval, only to a server that asks.
  // See onboarding::failure_context_pacer for the rules.
  onboarding::failure_context_pacer failure_pacer_;
  // Said once until the condition clears: reports that carry no metrics
  // because capture is off, a report too large for the server, a server
  // without the route, a header that cannot be read, and the last failed
  // report.
  bool failure_no_metrics_logged_ = false;
  bool failure_route_missing_logged_ = false;
  bool bad_failure_header_logged_ = false;
  unsigned int last_failure_report_error_ = 0;
  reload_function request_reload_;
  fleet_seams seams_;

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
  // The management url is not https, and the operator opted into that. Kept so
  // do_call does not attach a client certificate and a pin to a plain socket,
  // which the http client refuses outright.
  bool plaintext_channel_ = false;
  std::string last_transport_error_;
  unsigned long transport_failures_ = 0;

  std::shared_ptr<boost::thread> thread_;
};
