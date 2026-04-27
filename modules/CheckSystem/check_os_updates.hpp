/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
  std::string title;          // Update title
  std::string severity;       // MsrcSeverity (Critical, Important, Moderate, Low, "")
  std::string category;       // Primary category name (e.g. "Security Updates", "Critical Updates")
  bool is_security;           // True for "Security Updates" category
  bool is_critical;           // True for "Critical Updates" category
  bool reboot_required;       // True if installing this update requires a reboot

  update_info() : is_security(false), is_critical(false), reboot_required(false) {}

  std::string get_title() const { return title; }
  std::string get_severity() const { return severity; }
  std::string get_category() const { return category; }
  std::string get_is_security() const { return is_security ? "true" : "false"; }
  std::string get_is_critical() const { return is_critical ? "true" : "false"; }
  std::string get_reboot_required() const { return reboot_required ? "true" : "false"; }
};

// Aggregated update information used as the filter object.
struct os_updates_obj {
  long long count;             // Total number of available updates
  long long security;          // Number of security updates
  long long critical;          // Number of critical updates
  long long important;         // Number of MsrcSeverity == "Important" updates
  long long reboot_required;   // Number of updates requiring a reboot
  bool fetch_succeeded;        // False if the WUA query has not produced any data yet
  std::string error;           // Last error encountered while fetching (if any)
  std::vector<update_info> updates;

  os_updates_obj() : count(0), security(0), critical(0), important(0), reboot_required(0), fetch_succeeded(false) {}

  long long get_count() const { return count; }
  long long get_security() const { return security; }
  long long get_critical() const { return critical; }
  long long get_important() const { return important; }
  long long get_reboot_required() const { return reboot_required; }
  std::string get_titles() const;
  std::string get_status() const;
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
  long long last_fetch_;     // last time fetch ran successfully (epoch seconds), -1 = never
  long long ttl_seconds_;    // minimum seconds between WUA searches (default: 1 hour)
  bool fetch_supported_;     // false once the WUA API has been determined unavailable

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
