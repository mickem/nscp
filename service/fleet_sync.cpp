// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "fleet_sync.hpp"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <bytes/unzip.hpp>
#include <ctime>
#include <fstream>
#include <net/http/client.hpp>
#include <random>
#include <sstream>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <utility>

namespace fs = boost::filesystem;
namespace json = boost::json;

namespace {

const char *log_module = "fleet";

const char *desired_state_path = "/agent/v1/desired-state";
const char *state_report_path = "/agent/v1/state-report";
const char *metrics_path = "/agent/v1/metrics";
const char *renew_path = "/agent/v1/renew";
const char *heartbeat_path = "/agent/v1/heartbeat";
// Renew when the client certificate has fewer days than this left.
const long renew_threshold_days = 14;
// Upper bound on the samples submitted per poll. A module publishing a metric
// per process (or per file system, per interface, ...) can produce a lot of
// them; the fleet server is not a time series database.
const std::size_t max_metric_samples = 1000;
// Longest we will ever sleep between polls, whatever the server asks for
// (Retry-After on a 429). parse_desired_state clamps its own hint; this covers
// the header, which nothing else validates.
const unsigned long max_sleep_seconds = 86400;
// Staged script limits. Bundles are signed, so this is not the primary
// defense - but a signed bundle is still a zip, and an agent that unpacks an
// unbounded archive into memory is a denial of service waiting to happen.
const std::size_t max_script_bytes = 16u * 1024u * 1024u;
const std::size_t max_bundle_script_bytes = 64u * 1024u * 1024u;

// Clamp a sleep hint into something we are willing to act on.
unsigned long clamp_sleep_seconds(const unsigned long seconds) { return std::max(1ul, std::min(seconds, max_sleep_seconds)); }

std::string default_os() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "linux";
#endif
}

// ±10% jitter so a fleet does not poll in lockstep.
unsigned long with_jitter_ms(const unsigned long seconds) {
  std::random_device rd;
  const unsigned long base = seconds * 1000UL;
  const unsigned long spread = base / 5 + 1;  // 20% window centered on base
  return base * 9 / 10 + rd() % spread;
}

boost::optional<unsigned long> get_retry_after(const http::response &response) {
  const auto it = response.headers_.find("retry-after");
  if (it == response.headers_.end()) return boost::none;
  return onboarding::parse_retry_after(it->second);
}

// Reject zip entries that would escape the staging directory.
bool is_safe_entry(const std::string &name) {
  if (name.empty() || name[0] == '/' || name[0] == '\\') return false;
  if (name.find("..") != std::string::npos) return false;
  if (name.find(':') != std::string::npos) return false;
  return true;
}

void write_file(const fs::path &path, const std::string &data) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
  if (!out) throw onboarding::onboarding_error("Failed to write " + path.string(), false);
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!out) throw onboarding::onboarding_error("Failed to write " + path.string(), false);
}

// Copy a JSON string in full: c_str() would truncate at an embedded nul.
std::string json_string(const json::string &value) { return std::string(value.data(), value.size()); }

std::string read_file(const fs::path &path) {
  std::ifstream in(path.string().c_str(), std::ios::binary);
  if (!in) throw onboarding::onboarding_error("Failed to read " + path.string(), false);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

// Flatten a metrics bundle tree into "bundle.child.metric" samples - the same
// dotted naming the REST /api/v2/metrics view uses, so an operator sees the
// same keys locally and on the fleet server. Only numeric metrics can be
// expressed as a sample: strings, summaries and histograms are skipped.
void flatten_bundle(const PB::Metrics::MetricsBundle &bundle, const std::string &prefix, const long long timestamp,
                    std::vector<onboarding::metric_sample> &samples) {
  const std::string path = prefix.empty() ? bundle.key() : (bundle.key().empty() ? prefix : prefix + "." + bundle.key());
  for (const PB::Metrics::MetricsBundle &child : bundle.children()) {
    flatten_bundle(child, path, timestamp, samples);
  }
  for (const PB::Metrics::Metric &metric : bundle.value()) {
    onboarding::metric_sample sample;
    if (metric.has_gauge_value()) {
      sample.value = metric.gauge_value().value();
    } else if (metric.has_counter_value()) {
      sample.value = metric.counter_value().value();
    } else if (metric.has_untyped_value()) {
      sample.value = metric.untyped_value().value();
    } else {
      continue;
    }
    sample.key = path.empty() ? metric.key() : path + "." + metric.key();
    sample.ts = timestamp;
    samples.push_back(sample);
  }
}

// Move every regular file below `from` to the same relative path below `to`,
// replacing existing files. Directory-level atomicity is not possible with a
// shared target dir; per-file rename is the practical equivalent.
void promote_tree(const fs::path &from, const fs::path &to) {
  if (!fs::exists(from)) return;
  for (fs::recursive_directory_iterator it(from), end; it != end; ++it) {
    if (!fs::is_regular_file(it->status())) continue;
    const fs::path relative = fs::relative(it->path(), from);
    const fs::path target = to / relative;
    fs::create_directories(target.parent_path());
    fs::rename(it->path(), target);
  }
}

}  // namespace

bool fleet_sync::has_manifest(const std::string &state_file) {
  boost::system::error_code ignored;
  return fs::exists(state_file, ignored);
}

fleet_sync::fleet_sync(nsclient::logging::logger_instance logger, fleet_config config, nsclient::core::tag_repository_instance tags,
                       reload_function request_reload)
    : logger_(std::move(logger)),
      config_(std::move(config)),
      tags_(std::move(tags)),
      request_reload_(std::move(request_reload)),
      started_(std::chrono::steady_clock::now()) {
  thread_ = std::make_shared<boost::thread>([this] { this->thread_proc(); });
}

fleet_sync::~fleet_sync() {
  try {
    stop();
  } catch (...) {
  }
}

void fleet_sync::stop() {
  if (thread_) {
    thread_->interrupt();
    thread_->join();
  }
  thread_.reset();
}

void fleet_sync::log_error(const std::string &message) const { logger_->error(log_module, __FILE__, __LINE__, message); }
void fleet_sync::log_info(const std::string &message) const { logger_->info(log_module, __FILE__, __LINE__, message); }
void fleet_sync::log_debug(const std::string &message) const { logger_->debug(log_module, __FILE__, __LINE__, message); }

http::response fleet_sync::do_call(const char *verb, const std::string &path, const std::string &payload) {
  const std::string full_url = identity_.mtls_url + path;
  const http::parsed_url parsed = http::parse_url(full_url);
  http::http_client_options options(parsed.protocol, config_.tls_version, "none", "");
  options.identity_.cert_pem = identity_.cert_pem;
  options.identity_.key_pem = identity_.private_key_pem;
  options.identity_.pinned_ca_pem = identity_.mtls_server_cert_pem;
  // A fleet server that accepts the connection and then goes quiet must not
  // take this thread with it: without a deadline the loop stops polling for
  // good and the host silently drops out of management.
  options.timeout_seconds_ = config_.timeout_seconds;
  http::request rq(verb, parsed.host, parsed.path);
  rq.add_header("Accept", "application/json");
  if (!payload.empty()) {
    rq.add_post_payload("application/json", payload);
  }
  http::simple_client client(options);
  return client.fetch(parsed.host, parsed.port, rq);
}

void fleet_sync::note_transport_success() {
  if (transport_failures_ > 0) {
    log_info("Fleet server connection restored after " + str::xtos(transport_failures_) + " failed attempt(s)");
  }
  transport_failures_ = 0;
  last_transport_error_.clear();
  transport_ok_ = true;
}

void fleet_sync::log_transport_failure(const std::string &operation, const std::string &message) {
  transport_ok_ = false;
  ++transport_failures_;

  const onboarding::transport_error_info info = onboarding::classify_transport_error(message);
  std::string advice = info.advice;
  if (info.kind == onboarding::transport_error_kind::tls_identity) {
    // Per the protocol's error table: a rejected identity is only actionable
    // (re-enroll) - but say WHY when we can tell the cert simply expired.
    try {
      if (onboarding::days_until_expiry(identity_.cert_pem) < 0) {
        advice = "This host's client certificate has expired (host offline past its renewal window): re-enroll with a new install command (nscp enroll ... --force).";
      }
    } catch (...) {
    }
  }

  std::string full = operation + " failed: " + message;
  if (!advice.empty()) full += " - " + advice;

  if (message != last_transport_error_) {
    // A new (or changed) failure is worth an error entry with full guidance.
    last_transport_error_ = message;
    log_error(full);
  } else if (transport_failures_ % 10 == 0) {
    // Same failure repeating: stay quiet apart from a periodic reminder.
    log_info("Fleet server still unreachable (" + str::xtos(transport_failures_) + " consecutive failures): " + message);
  } else {
    log_debug(full);
  }
}

std::map<std::string, std::string> fleet_sync::collect_tags() const {
  // Module-contributed tags first, then the protocol built-ins on top: os,
  // hostname and nscp_version drive server-side group selectors, so a module
  // must not be able to shadow them.
  std::map<std::string, std::string> tags = tags_ ? tags_->get_all() : std::map<std::string, std::string>();
  tags["os"] = default_os();
  if (!config_.hostname.empty()) tags["hostname"] = config_.hostname;
  if (!config_.nscp_version.empty()) tags["nscp_version"] = config_.nscp_version;
  return tags;
}

void fleet_sync::load_applied_state() {
  const fs::path file = fs::path(config_.managed_path) / "applied-state.json";
  try {
    if (!fs::exists(file)) return;
    const json::object root = json::parse(read_file(file)).as_object();
    const json::value *hash = root.if_contains("state_hash");
    if (hash != nullptr && hash->is_string()) current_hash_ = json_string(hash->as_string());
    const json::value *bundles = root.if_contains("bundles");
    if (bundles != nullptr && bundles->is_array()) {
      for (const json::value &entry : bundles->as_array()) {
        if (!entry.is_object()) continue;
        onboarding::installed_bundle bundle;
        const json::object &o = entry.as_object();
        if (const json::value *v = o.if_contains("id")) {
          if (v->is_string()) bundle.id = json_string(v->as_string());
        }
        if (const json::value *v = o.if_contains("version")) {
          if (v->is_string()) bundle.version = json_string(v->as_string());
        }
        installed_.push_back(bundle);
      }
    }
  } catch (const std::exception &e) {
    log_error("Ignoring corrupt applied-state file " + file.string() + ": " + utf8::utf8_from_native(e.what()));
    current_hash_.clear();
    installed_.clear();
  }
}

void fleet_sync::save_applied_state() const {
  json::object root;
  root["version"] = 1;
  root["state_hash"] = current_hash_;
  json::array bundles;
  for (const onboarding::installed_bundle &bundle : installed_) {
    json::object entry;
    entry["id"] = bundle.id;
    entry["version"] = bundle.version;
    bundles.push_back(entry);
  }
  root["bundles"] = bundles;
  write_file(fs::path(config_.managed_path) / "applied-state.json", json::serialize(root));
}

void fleet_sync::report_state(const boost::optional<std::string> &applied_hash, const std::vector<std::string> &errors) {
  try {
    // Capture the revision BEFORE reading the tags: if a module updates a tag
    // while this report is in flight, the next loop iteration re-reports.
    const unsigned long long tag_revision = tags_ ? tags_->get_revision() : 0;
    const std::string body = onboarding::build_state_report(applied_hash, installed_, errors, collect_tags());
    const http::response response = do_call("POST", state_report_path, body);
    note_transport_success();
    if (!response.is_2xx()) {
      log_error("Failed to submit state report: " + str::xtos(response.status_code_) + " " + response.payload_);
      return;
    }
    reported_tag_revision_ = tag_revision;
    tags_reported_ = true;
  } catch (const std::exception &e) {
    log_transport_failure("State report", utf8::utf8_from_native(e.what()));
  }
}

void fleet_sync::on_metrics(const PB::Metrics::MetricsMessage &metrics) {
  if (!config_.metrics) return;
  // The snapshot is timestamped where it is taken, not where it is sent: it
  // waits here until the next poll, which can be a poll interval away.
  const long long timestamp = static_cast<long long>(std::time(nullptr));
  std::vector<onboarding::metric_sample> samples;
  for (const PB::Metrics::MetricsMessage::Response &payload : metrics.payload()) {
    for (const PB::Metrics::MetricsBundle &bundle : payload.bundles()) {
      flatten_bundle(bundle, "", timestamp, samples);
    }
  }
  if (samples.size() > max_metric_samples) {
    const std::string message = "Metrics snapshot truncated to " + str::xtos(max_metric_samples) + " of " + str::xtos(samples.size()) +
                                " samples: reduce the number of metric-producing modules to report all of them";
    if (!metrics_truncated_logged_) {
      metrics_truncated_logged_ = true;
      log_info(message);
    } else {
      log_debug(message);
    }
    samples.resize(max_metric_samples);
  }
  boost::mutex::scoped_lock lock(metrics_mutex_);
  metrics_.swap(samples);
}

void fleet_sync::post_metrics() {
  try {
    std::vector<onboarding::metric_sample> samples;
    {
      boost::mutex::scoped_lock lock(metrics_mutex_);
      samples = metrics_;
    }
    // Agent uptime is ours to report: it is the one thing no module knows.
    onboarding::metric_sample uptime;
    uptime.key = "uptime_seconds";
    uptime.value = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started_).count());
    samples.push_back(uptime);
    const http::response response = do_call("POST", metrics_path, onboarding::build_metrics(samples));
    note_transport_success();
    if (!response.is_2xx()) {
      log_info("Failed to submit metrics: " + str::xtos(response.status_code_));
    }
  } catch (const std::exception &e) {
    log_transport_failure("Metrics submission", utf8::utf8_from_native(e.what()));
  }
}

void fleet_sync::maybe_renew() {
  long days = 0;
  try {
    days = onboarding::days_until_expiry(identity_.cert_pem);
  } catch (const std::exception &e) {
    log_error("Cannot check certificate expiry: " + utf8::utf8_from_native(e.what()));
    return;
  }
  if (days > renew_threshold_days) return;
  log_info("Client certificate expires in " + str::xtos(days) + " days: renewing");
  try {
    const onboarding::identity fresh = onboarding::generate_identity();
    json::object body;
    body["csr_pem"] = fresh.csr_pem;
    const http::response response = do_call("POST", renew_path, json::serialize(body));
    if (!response.is_2xx()) {
      log_error("Certificate renewal failed: " + str::xtos(response.status_code_) + " " + response.payload_);
      return;
    }
    const onboarding::enrolled_identity renewed = onboarding::parse_renew_response(response.payload_, fresh, identity_);
    onboarding::save_state(renewed, config_.state_file);
    identity_ = renewed;
    log_info("Client certificate renewed");
  } catch (const std::exception &e) {
    log_error("Certificate renewal failed: " + utf8::utf8_from_native(e.what()));
  }
}

bool fleet_sync::fetch_bundle(const onboarding::bundle_info &bundle, std::string &bytes, std::string &error, bool &gone) {
  gone = false;
  const fs::path cache_dir = fs::path(config_.managed_path) / "cache";
  const fs::path cached = cache_dir / (bundle.id + "-" + bundle.sha256.substr(0, 16) + ".zip");
  if (fs::exists(cached)) {
    bytes = read_file(cached);
    return true;
  }
  http::response response;
  try {
    response = do_call("GET", bundle.url);
  } catch (const std::exception &e) {
    error = "Failed to download bundle " + bundle.id + ": " + utf8::utf8_from_native(e.what());
    return false;
  }
  if (response.status_code_ == 403) {
    // The server recomputed membership and this bundle is no longer ours:
    // the desired state changed under us. Abandon the cycle and re-poll.
    gone = true;
    error = "Bundle " + bundle.id + " no longer in effective set";
    return false;
  }
  if (!response.is_2xx()) {
    error = "Failed to download bundle " + bundle.id + ": " + str::xtos(response.status_code_);
    return false;
  }
  bytes = response.payload_;
  // Cache only verified content so a poisoned download can never be replayed
  // from disk.
  std::string verify_error;
  if (!onboarding::verify_bundle(identity_.bundle_signing_pub_pem, bytes, bundle.sha256, bundle.signature, verify_error)) {
    error = "Bundle " + bundle.id + " failed verification: " + verify_error;
    return false;
  }
  write_file(cached, bytes);
  return true;
}

bool fleet_sync::apply_state(const onboarding::desired_state &state, std::vector<std::string> &errors, bool &stale) {
  stale = false;
  const fs::path managed(config_.managed_path);
  const fs::path staging = managed / "staging";
  std::vector<onboarding::installed_bundle> new_installed;
  try {
    boost::system::error_code ignored;
    fs::remove_all(staging, ignored);
    fs::create_directories(staging);

    json::value merged = json::parse(state.merged_config_json.empty() ? "{}" : state.merged_config_json);

    for (const onboarding::bundle_info &bundle : state.bundles) {
      std::string bytes, error;
      bool gone = false;
      if (!fetch_bundle(bundle, bytes, error, gone)) {
        stale = gone;
        errors.push_back(error);
        return false;
      }
      // Verify even cache hits: cheap, and protects against on-disk tampering.
      std::string verify_error;
      if (!onboarding::verify_bundle(identity_.bundle_signing_pub_pem, bytes, bundle.sha256, bundle.signature, verify_error)) {
        errors.push_back("Bundle " + bundle.id + " failed verification: " + verify_error);
        return false;
      }

      // The unzip reader is file-based; the verified cache file is the source.
      const fs::path cached = managed / "cache" / (bundle.id + "-" + bundle.sha256.substr(0, 16) + ".zip");
      bytes::unzip::reader reader(cached.string());
      if (!reader.is_open()) {
        errors.push_back("Bundle " + bundle.id + " is not a readable zip archive");
        return false;
      }
      std::string config_json;
      if (reader.extract("config.json", config_json)) {
        try {
          merged = onboarding::json_merge_patch(merged, json::parse(config_json));
        } catch (const std::exception &e) {
          errors.push_back("Bundle " + bundle.id + " config.json is invalid: " + utf8::utf8_from_native(e.what()));
          return false;
        }
      }
      // Stage script files; applying in ascending priority order means later
      // (higher-priority) bundles overwrite path collisions.
      std::size_t staged_bytes = 0;
      for (unsigned int i = 0; i < reader.size(); ++i) {
        bytes::unzip::file_entry entry;
        if (!reader.stat(i, entry)) continue;
        std::string name = entry.filename;
        std::replace(name.begin(), name.end(), '\\', '/');
        // Check every entry, not just the ones we would stage: an archive
        // containing an absolute path or a traversal is malformed however we
        // would have treated that entry, and rejecting the bundle is the only
        // answer that cannot be wrong.
        if (!is_safe_entry(name)) {
          errors.push_back("Bundle " + bundle.id + " contains unsafe path: " + entry.filename);
          return false;
        }
        if (name.compare(0, 8, "scripts/") != 0 || name.back() == '/') continue;
        // Check the declared size before unpacking anything: the point is to
        // never allocate the archive's claim in the first place.
        staged_bytes += entry.uncompressed_size;
        if (entry.uncompressed_size > max_script_bytes || staged_bytes > max_bundle_script_bytes) {
          errors.push_back("Bundle " + bundle.id + " scripts exceed the size limit (" + str::xtos(max_script_bytes / (1024 * 1024)) + "MB per file, " +
                           str::xtos(max_bundle_script_bytes / (1024 * 1024)) + "MB per bundle)");
          return false;
        }
        std::string content;
        if (!reader.extract(entry.filename, content)) {
          errors.push_back("Bundle " + bundle.id + ": failed to extract " + entry.filename);
          return false;
        }
        write_file(staging / name, content);
      }
      onboarding::installed_bundle installed;
      installed.id = bundle.id;
      installed.version = bundle.version;
      new_installed.push_back(installed);
    }

    write_file(staging / "fleet.ini", onboarding::render_ini(merged));

    // Swap: fleet.ini atomically, staged scripts file-by-file. The managed
    // scripts tree is wiped first so scripts dropped from the desired state
    // (removed bundle, renamed file) actually disappear instead of lingering.
    fs::rename(staging / "fleet.ini", managed / "fleet.ini");
    fs::remove_all(managed / "scripts", ignored);
    promote_tree(staging / "scripts", managed / "scripts");
    fs::remove_all(staging, ignored);

    installed_ = new_installed;
    current_hash_ = state.state_hash;
    save_applied_state();
    return true;
  } catch (const std::exception &e) {
    errors.push_back(std::string("Apply failed: ") + utf8::utf8_from_native(e.what()));
    return false;
  }
}

unsigned long fleet_sync::poll_once() {
  std::string path = desired_state_path;
  if (!current_hash_.empty()) {
    path += "?current_hash=" + current_hash_;
  }
  http::response response;
  try {
    response = do_call("GET", path);
  } catch (const std::exception &e) {
    ++failures_;
    log_transport_failure("Desired-state poll", utf8::utf8_from_native(e.what()));
    const unsigned long backoff = poll_interval_ << std::min(failures_, 5u);
    return std::min(backoff, poll_interval_ * 5);
  }
  note_transport_success();

  if (response.status_code_ == 304) {
    failures_ = 0;
    const boost::optional<unsigned long> next = onboarding::parse_next_poll(response.payload_);
    if (next) poll_interval_ = std::max(1ul, *next);
    return poll_interval_;
  }
  if (response.status_code_ == 429) {
    const boost::optional<unsigned long> retry = get_retry_after(response);
    log_info("Desired-state poll rate limited");
    return retry ? clamp_sleep_seconds(*retry) : poll_interval_;
  }
  if (!response.is_2xx()) {
    ++failures_;
    log_error("Desired-state poll failed: " + str::xtos(response.status_code_) + " " + response.payload_);
    const unsigned long backoff = poll_interval_ << std::min(failures_, 5u);
    return std::min(backoff, poll_interval_ * 5);
  }

  failures_ = 0;
  try {
    const onboarding::desired_state state = onboarding::parse_desired_state(response.payload_);
    poll_interval_ = std::max(1ul, state.next_poll_in_seconds);
    log_debug("New desired state " + state.state_hash + " with " + str::xtos(state.bundles.size()) + " bundle(s)");

    std::vector<std::string> errors;
    bool stale = false;
    if (apply_state(state, errors, stale)) {
      log_info("Applied fleet configuration " + state.state_hash);
      report_state(current_hash_, errors);
      // The rendered fleet.ini is part of the configuration; the core reloads
      // it on its own schedule (delayed), not from this thread.
      request_reload_();
    } else if (stale) {
      // The desired state changed server-side while we were applying it; this
      // is not a configuration failure, so nothing is reported. The next poll
      // fetches the new state.
      for (const std::string &error : errors) log_info(error + " - abandoning stale desired state " + state.state_hash);
    } else {
      for (const std::string &error : errors) log_error(error);
      // Per protocol: never report a hash that was not fully applied.
      report_state(boost::none, errors);
    }
  } catch (const std::exception &e) {
    log_error("Failed to process desired state: " + utf8::utf8_from_native(e.what()));
    std::vector<std::string> errors;
    errors.push_back(std::string("Failed to process desired state: ") + utf8::utf8_from_native(e.what()));
    report_state(boost::none, errors);
  }
  return poll_interval_;
}

void fleet_sync::thread_proc() {
  try {
    const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(config_.state_file);
    if (!loaded) {
      log_error("No fleet enrollment found (" + config_.state_file + "): run `nscp enroll` first; fleet sync disabled");
      return;
    }
    identity_ = *loaded;
    while (!identity_.mtls_url.empty() && identity_.mtls_url.back() == '/') identity_.mtls_url.pop_back();
    fs::create_directories(fs::path(config_.managed_path));
    load_applied_state();

    // Startup: cheap identity check, then report tags early so a fresh host
    // lands in its groups before the first desired-state poll.
    try {
      const http::response heartbeat = do_call("GET", heartbeat_path);
      note_transport_success();
      if (heartbeat.status_code_ == 403) {
        log_error("Fleet server rejected our certificate (revoked): re-enroll with a new install command; fleet sync disabled");
        return;
      }
    } catch (const std::exception &e) {
      log_transport_failure("Fleet heartbeat", utf8::utf8_from_native(e.what()));
    }
    report_state(current_hash_.empty() ? boost::optional<std::string>() : boost::optional<std::string>(current_hash_), std::vector<std::string>());

    while (true) {
      const unsigned long sleep_seconds = poll_once();
      // When the server is unreachable the poll already logged it; don't pile
      // further failures (and further log entries) on top with renewal or
      // metrics calls that cannot succeed either.
      if (transport_ok_) {
        maybe_renew();
        if (config_.metrics) post_metrics();
        // Module tags changed since the last successful report (e.g. a module
        // was (re)loaded and re-detected its facts): re-report so the server
        // updates group membership promptly. Identical tag maps are a server-
        // side no-op, and the revision only moves on real changes, so this
        // stays quiet in steady state.
        if (tags_ && (!tags_reported_ || tags_->get_revision() != reported_tag_revision_)) {
          report_state(current_hash_.empty() ? boost::optional<std::string>() : boost::optional<std::string>(current_hash_), std::vector<std::string>());
        }
      }
      boost::this_thread::sleep_for(boost::chrono::milliseconds(with_jitter_ms(sleep_seconds)));
    }
  } catch (const boost::thread_interrupted &) {
    // normal shutdown
  } catch (const std::exception &e) {
    log_error("Fleet sync thread died: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    log_error("Fleet sync thread died: UNKNOWN");
  }
}
