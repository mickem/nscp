// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <net/dll_defines.hpp>

#include <mutex>
#include <boost/asio/ip/address.hpp>
#include <boost/optional.hpp>
#include <list>
#include <string>

namespace socket_helpers {

struct allowed_hosts_manager {
  template <class addr_type_t>
  struct host_record {
    host_record(const std::string &host, const addr_type_t addr, const addr_type_t mask) : host(host), addr(addr), mask(mask) {}
    host_record(const host_record &other) = default;
    host_record &operator=(const host_record &other) = default;
    std::string host;
    addr_type_t addr;
    addr_type_t mask;
  };
  typedef boost::asio::ip::address_v4::bytes_type addr_v4;
  typedef boost::asio::ip::address_v6::bytes_type addr_v6;

  typedef host_record<addr_v4> host_record_v4;
  typedef host_record<addr_v6> host_record_v6;

  std::list<host_record_v4> entries_v4;
  std::list<host_record_v6> entries_v6;
  std::list<std::string> sources;
  bool cached;
  // The settings section `allowed hosts` is read from (`/settings/NRPE/server`,
  // ...), so a rejection can say where to fix it. Empty when the owner never
  // set it; the message then names the setting without a path. Written with
  // set_settings_path(): a reload rewrites it while connections are checked.
  std::string settings_path;
  // set_source()/refresh() rewrite the lists on the settings thread while
  // is_allowed() walks them on every accepting thread. A plain mutex, not a
  // shared_mutex: installer_lib compiles this header as its own C++14 project
  // for the XP-compatible custom actions (it even defines
  // BOOST_NO_CXX17_HDR_SHARED_MUTEX), so std::shared_mutex is not available
  // here. The critical section is a short list walk per accepted connection,
  // so there is nothing to win from a reader/writer lock anyway.
  mutable std::mutex entries_mutex_;

  allowed_hosts_manager() : cached(true) {}
  allowed_hosts_manager(const allowed_hosts_manager &other)
      : entries_v4(other.entries_v4), entries_v6(other.entries_v6), sources(other.sources), cached(other.cached), settings_path(other.settings_path) {}
  allowed_hosts_manager &operator=(const allowed_hosts_manager &other) {
    if (this != &other) {
      entries_v4 = other.entries_v4;
      entries_v6 = other.entries_v6;
      sources = other.sources;
      cached = other.cached;
      settings_path = other.settings_path;
    }
    return *this;
  }

  NSCP_NET_EXPORT void set_source(const std::string &source);
  void set_settings_path(const std::string &path) {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    settings_path = path;
  }
  addr_v4 lookup_mask_v4(std::string mask);
  addr_v6 lookup_mask_v6(std::string mask);
  NSCP_NET_EXPORT void refresh(std::list<std::string> &errors);
  // Adds the entries for one `allowed hosts` record once its address part is
  // known: parsed, or resolved for a host name. `prefix` is the record's `/N`,
  // disengaged when it has none. All or nothing: when the prefix is longer
  // than any one address's family allows, the record is reported and none of
  // its addresses is added. Called from refresh() with the lock held; public
  // so a test can hand it a mixed-family resolution without DNS.
  NSCP_NET_EXPORT void add_record(const std::string &record, const boost::optional<std::size_t> &prefix,
                                  const std::list<boost::asio::ip::address> &addresses, std::list<std::string> &errors);

  template <class T>
  static bool match_host(const T &allowed, const T &mask, const T &remote) {
    for (std::size_t i = 0; i < allowed.size(); i++) {
      if ((allowed[i] & mask[i]) != (remote[i] & mask[i])) return false;
    }
    return true;
  }
  bool is_allowed(const boost::asio::ip::address &address, std::list<std::string> &errors) {
    // Fail closed when the allowed-hosts list is empty. Per-module defaults
    // (typically `127.0.0.1`) mean an empty list almost always represents an
    // operator who explicitly cleared the setting - either to deliberately
    // expose the agent (now requires saying so explicitly) or by mistake
    // (now visible instead of silently opening the listener).
    //
    // BREAKING CHANGE from earlier versions: deployments that relied on
    // `allowed hosts =` (empty) to accept any source must set
    // `allowed hosts = 0.0.0.0/0,::/0` to keep the same behaviour.
    //
    // `errors` only collects problems with the list itself (an entry that does
    // not parse or resolve when it is re-read here). Why this address was
    // turned away is rejection_message()'s job, so the caller can log one line
    // that says it all.
    if (!cached) refresh(errors);
    std::lock_guard<std::mutex> lock(entries_mutex_);
    if (entries_v4.empty() && entries_v6.empty()) return false;
    if (address.is_v4()) {
      return is_allowed_v4(address.to_v4().to_bytes());
    }
    if (address.is_v6()) {
      const auto v6 = address.to_v6();
      // A v4 client on a dual-stack listener: match it as native v6 and as v4.
      if (v6.is_v4_mapped()) {
        return is_allowed_v6(v6.to_bytes()) || is_allowed_v4(boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, v6).to_bytes());
      }
      return is_allowed_v6(v6.to_bytes());
    }
    return false;
  }
  // Called from is_allowed() with the lock held.
  bool is_allowed_v4(const addr_v4 &remote) const {
    for (const host_record_v4 &r : entries_v4) {
      if (match_host(r.addr, r.mask, remote)) return true;
    }
    return false;
  }
  bool is_allowed_v6(const addr_v6 &remote) const {
    for (const host_record_v6 &r : entries_v6) {
      if (match_host(r.addr, r.mask, remote)) return true;
    }
    return false;
  }
  // The one line to log when is_allowed() returned false: who was rejected,
  // why (not in the list, or the list is empty / unusable), what the list
  // holds and where to change it. `errors` is what is_allowed() collected and
  // is appended so a broken entry is not reported separately.
  NSCP_NET_EXPORT std::string rejection_message(const boost::asio::ip::address &address, const std::list<std::string> &errors) const;
  NSCP_NET_EXPORT std::string to_string() const;
};
}  // namespace socket_helpers
