// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <vector>

namespace os_updates_check {

// A single available update, populated from IUpdate properties returned by the
// Windows Update Agent (WUA) IUpdateSearcher.
struct update_info {
  std::string title;     // Update title
  std::string severity;  // MsrcSeverity (Critical, Important, Moderate, Low, "")
  std::string category;  // Primary category name (e.g. "Security Updates", "Critical Updates")
  bool is_security;      // True for "Security Updates" category
  bool is_critical;      // True for "Critical Updates" category
  bool is_defender;      // True for Defender/definition updates (churn daily; thresholded separately)
  bool is_rollup;        // True for monthly/quality "Update Rollup" category
  bool reboot_required;  // True if installing this update requires a reboot

  update_info() : is_security(false), is_critical(false), is_defender(false), is_rollup(false), reboot_required(false) {}

  std::string get_title() const { return title; }
  std::string get_severity() const { return severity; }
  std::string get_category() const { return category; }
  std::string get_is_security() const { return is_security ? "true" : "false"; }
  std::string get_is_critical() const { return is_critical ? "true" : "false"; }
  std::string get_is_defender() const { return is_defender ? "true" : "false"; }
  std::string get_is_rollup() const { return is_rollup ? "true" : "false"; }
  std::string get_reboot_required() const { return reboot_required ? "true" : "false"; }
};

// Aggregated update information used as the filter object.
struct os_updates_obj {
  long long count;            // Total number of available updates
  long long security;         // Number of security updates
  long long critical;         // Number of critical updates
  long long important;        // Number of MsrcSeverity == "Important" updates
  long long defender;         // Number of Defender/definition updates
  long long rollups;          // Number of update-rollup updates
  long long reboot_required;  // Number of updates requiring a reboot
  bool fetch_succeeded;       // False if the WUA query has not produced any data yet
  bool reboot_pending;        // System-wide RebootRequired flag (registry), independent of the update list
  std::string error;          // Last error encountered while fetching (if any)
  std::vector<update_info> updates;

  os_updates_obj()
      : count(0), security(0), critical(0), important(0), defender(0), rollups(0), reboot_required(0), fetch_succeeded(false), reboot_pending(false) {}

  long long get_count() const { return count; }
  long long get_security() const { return security; }
  long long get_critical() const { return critical; }
  long long get_important() const { return important; }
  long long get_defender() const { return defender; }
  long long get_rollups() const { return rollups; }
  long long get_reboot_required() const { return reboot_required; }
  long long get_reboot_pending() const { return reboot_pending ? 1 : 0; }
  std::string get_titles() const;
  std::string get_update_status() const;
  std::string get_error() const { return error; }

  std::string show() const;

  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  // Recompute the aggregate counters from the updates vector.
  void recompute();
};

// Helpers exposed for unit testing.
//
// Classify an update based on its category list and MSRC severity. The category
// argument is the first / primary category name; the severity argument is the
// IUpdate::MsrcSeverity property as returned by WUA.
void classify_update(const std::string &category, const std::string &severity, update_info &out);

// Cached fetcher backed by the Windows Update Agent (WUA) COM API.
//
// The WUA search is slow (often 30+ seconds) and must not run on the per-second
// collector tick. ttl_seconds_ controls how often a fresh search is performed.
class os_updates_data final {
  boost::shared_mutex mutex_;
  os_updates_obj data_;
  long long last_fetch_;   // last time fetch ran successfully (epoch seconds), -1 = never
  long long ttl_seconds_;  // minimum seconds between WUA searches (default: 1 hour)
  bool fetch_supported_;   // false once the WUA API has been determined unavailable

 public:
  os_updates_data();

  void set_ttl_seconds(long long ttl) { ttl_seconds_ = ttl; }
  long long get_ttl_seconds() const { return ttl_seconds_; }

  // Issue a WUA search if the cache is older than the configured TTL.
  // Safe to call from the collector loop; no-op until TTL has elapsed.
  void fetch();

  // Force a search regardless of TTL. Used on the first collector tick and
  // exposed for tests.
  void force_fetch();

  // Return a snapshot of the current data.
  os_updates_obj get();
};

namespace check {
void check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, os_updates_obj data);
}  // namespace check

}  // namespace os_updates_check
