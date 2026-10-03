// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio/ip/address.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <net/dll_defines.hpp>
#include <string>

namespace boost {
class thread;
}

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
  // set_source()/refresh() rewrite the lists on the settings thread while
  // is_allowed() walks them on every accepting thread. A plain mutex, not a
  // shared_mutex: installer_lib compiles this header as its own C++14 project
  // for the XP-compatible custom actions (it even defines
  // BOOST_NO_CXX17_HDR_SHARED_MUTEX), so std::shared_mutex is not available
  // here. The critical section is a short list walk per accepted connection,
  // so there is nothing to win from a reader/writer lock anyway.
  mutable std::mutex entries_mutex_;
  // Orders refreshes that overlap (with `cached` off, every accepting thread
  // runs one): each takes a number when it snapshots the sources, and one
  // that finishes after a later-started one has already published is
  // dropped, so a slow, failing lookup cannot bring back addresses a newer
  // refresh removed. Guarded by entries_mutex_.
  std::uint64_t refresh_started_ = 0;
  std::uint64_t refresh_published_ = 0;

  typedef std::function<void(const std::list<std::string> &errors)> error_reporter;
  struct background_refresh;
  // See start_background_refresh(). Not copied: a copy is a separate list.
  // The only handle to it: dropping it (the implicit destructor) stops the
  // refresh, through a deleter set in allowed_hosts.cpp - so code that never
  // starts one does not have to link that file.
  std::shared_ptr<background_refresh> background_;

  allowed_hosts_manager() : cached(true) {}
  allowed_hosts_manager(const allowed_hosts_manager &other)
      : entries_v4(other.entries_v4), entries_v6(other.entries_v6), sources(other.sources), cached(other.cached) {}
  allowed_hosts_manager &operator=(const allowed_hosts_manager &other) {
    if (this != &other) {
      entries_v4 = other.entries_v4;
      entries_v6 = other.entries_v6;
      sources = other.sources;
      cached = other.cached;
    }
    return *this;
  }

  NSCP_NET_EXPORT void set_source(const std::string &source);
  addr_v4 lookup_mask_v4(std::string mask);
  addr_v6 lookup_mask_v6(std::string mask);
  NSCP_NET_EXPORT void refresh(std::list<std::string> &errors);

  // Re-resolve the host names every `interval` on a thread of its own, so a
  // listener can keep names current without a DNS lookup on the thread that
  // accepts connections (which is what `cached = false` does). `report` gets
  // the errors of each round; it is never called once the refresh is stopped.
  // Replaces a refresh already running.
  NSCP_NET_EXPORT void start_background_refresh(std::chrono::seconds interval, error_reporter report);
  // Stops the background refresh without waiting for it: a round stuck in a
  // slow resolver finishes on its own and then neither touches this object
  // nor reports. Returns the thread (null when none ran) so an owner that is
  // about to unload the code it runs can join it; dropping it is fine
  // otherwise. Destroying the manager does the same.
  NSCP_NET_EXPORT std::shared_ptr<boost::thread> stop_background_refresh();

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
    if (!cached) refresh(errors);
    std::lock_guard<std::mutex> lock(entries_mutex_);
    if (entries_v4.empty() && entries_v6.empty()) {
      errors.emplace_back("allowed_hosts is empty - rejecting all connections (set `allowed hosts = 0.0.0.0/0,::/0` to allow all)");
      return false;
    }
    if (address.is_v4()) {
      return is_allowed_v4(address.to_v4().to_bytes(), errors);
    }
    if (address.is_v6()) {
      const auto v6 = address.to_v6();
      if (v6.is_v4_mapped()) {
        // Try the address both as native v6 and via its v4-mapped form. Each
        // check appends "not allowed" on failure, so buffer their diagnostics
        // and only surface them if neither match succeeds - otherwise a match
        // on the fallback would leave a misleading rejection in `errors`.
        std::list<std::string> errors_v6;
        if (is_allowed_v6(v6.to_bytes(), errors_v6)) return true;
        std::list<std::string> errors_v4;
        if (is_allowed_v4(boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, v6).to_bytes(), errors_v4)) return true;
        errors.splice(errors.end(), errors_v6);
        errors.splice(errors.end(), errors_v4);
        return false;
      }
      return is_allowed_v6(v6.to_bytes(), errors);
    }
    return false;
  }
  // Called from is_allowed() with the lock held.
  bool is_allowed_v4(const addr_v4 &remote, std::list<std::string> &errors) {
    for (const host_record_v4 &r : entries_v4) {
      if (match_host(r.addr, r.mask, remote)) return true;
    }
    errors.emplace_back("IP address not allowed");
    return false;
  }
  bool is_allowed_v6(const addr_v6 &remote, std::list<std::string> &errors) {
    for (const host_record_v6 &r : entries_v6) {
      if (match_host(r.addr, r.mask, remote)) return true;
    }
    errors.emplace_back("IP address not allowed");
    return false;
  }
  NSCP_NET_EXPORT std::string to_string() const;
};
}  // namespace socket_helpers
