/*
 * Copyright (C) 2004-2016 Michael Medin
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

// #include <str/xtos.hpp>

#include <boost/asio/ip/address.hpp>

#include <list>
#include <string>

namespace socket_helpers {

struct allowed_hosts_manager {
  template <class addr_type_t>
  struct host_record {
    host_record(std::string host, addr_type_t addr, addr_type_t mask) : host(host), addr(addr), mask(mask) {}
    host_record(const host_record &other) : host(other.host), addr(other.addr), mask(other.mask) {}
    const host_record &operator=(const host_record &other) {
      host = other.host;
      addr = other.addr;
      mask = other.mask;
      return *this;
    }
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

  allowed_hosts_manager() : cached(true) {}
  allowed_hosts_manager(const allowed_hosts_manager &other)
      : entries_v4(other.entries_v4), entries_v6(other.entries_v6), sources(other.sources), cached(other.cached) {}
  const allowed_hosts_manager &operator=(const allowed_hosts_manager &other) {
    entries_v4 = other.entries_v4;
    entries_v6 = other.entries_v6;
    sources = other.sources;
    cached = other.cached;
    return *this;
  }

  void set_source(std::string source);
  addr_v4 lookup_mask_v4(std::string mask);
  addr_v6 lookup_mask_v6(std::string mask);
  void refresh(std::list<std::string> &errors);

  template <class T>
  inline bool match_host(const T &allowed, const T &mask, const T &remote) const {
    for (std::size_t i = 0; i < allowed.size(); i++) {
      if ((allowed[i] & mask[i]) != (remote[i] & mask[i])) return false;
    }
    return true;
  }
  bool is_allowed(const boost::asio::ip::address &address, std::list<std::string> &errors) {
    return (entries_v4.empty() && entries_v6.empty()) || (address.is_v4() && is_allowed_v4(address.to_v4().to_bytes(), errors)) ||
           (address.is_v6() && is_allowed_v6(address.to_v6().to_bytes(), errors)) ||
           (address.is_v6() && address.to_v6().is_v4_compatible() && is_allowed_v4(address.to_v6().to_v4().to_bytes(), errors)) ||
           (address.is_v6() && address.to_v6().is_v4_mapped() && is_allowed_v4(address.to_v6().to_v4().to_bytes(), errors));
  }
  bool is_allowed_v4(const addr_v4 &remote, std::list<std::string> &errors) {
    if (!cached) refresh(errors);
    for (const host_record_v4 &r : entries_v4) {
      if (match_host(r.addr, r.mask, remote)) return true;
    }
    return false;
  }
  bool is_allowed_v6(const addr_v6 &remote, std::list<std::string> &errors) {
    if (!cached) refresh(errors);
    for (const host_record_v6 &r : entries_v6) {
      if (match_host(r.addr, r.mask, remote)) return true;
    }
    return false;
  }
  std::string to_string();
};
}  // namespace socket_helpers
