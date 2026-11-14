// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <vector>

namespace net {

// Which IP version a network check should use. `any` leaves the choice to the
// resolver (the historical behaviour), the other two pin it, which is what a
// dual-stack host needs to assert "this service answers over IPv6" or to keep
// a check on IPv4 while an IPv6 path is known-broken.
enum class address_family { any, ipv4, ipv6 };

// Parse the value of an `address-family` argument. Returns false — leaving
// `out` untouched — for anything unrecognised, so the caller can report the bad
// value rather than silently falling back to `any`.
inline bool parse_address_family(const std::string &value, address_family &out) {
  std::string v;
  v.reserve(value.size());
  for (const char c : value) v += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);

  if (v.empty() || v == "any" || v == "both" || v == "0" || v == "unspec") {
    out = address_family::any;
    return true;
  }
  if (v == "ipv4" || v == "v4" || v == "4" || v == "inet") {
    out = address_family::ipv4;
    return true;
  }
  if (v == "ipv6" || v == "v6" || v == "6" || v == "inet6") {
    out = address_family::ipv6;
    return true;
  }
  return false;
}

inline std::string to_string(const address_family af) {
  switch (af) {
    case address_family::ipv4:
      return "ipv4";
    case address_family::ipv6:
      return "ipv6";
    default:
      return "any";
  }
}

// The help text for the `address-family` option, kept in one place so every
// check documents it identically.
inline const char *address_family_option_help() {
  return "IP version to use: any (default, let the resolver choose), ipv4 or ipv6. Accepts 4/v4/inet and 6/v6/inet6 as aliases.";
}

// Resolve host/service restricted to the requested address family. Templated on
// the resolver so it serves tcp, udp and icmp alike; `Resolver::protocol_type`
// supplies the v4()/v6() instances, which keeps this header free of any
// protocol-specific asio includes. With ipv6 pinned, v4-mapped answers are
// dropped (see below), so every platform resolves an IPv4-only host the same way.
template <typename Resolver>
typename Resolver::results_type resolve_for_family(Resolver &resolver, const address_family af, const std::string &host, const std::string &service,
                                                   boost::system::error_code &ec) {
  typedef typename Resolver::protocol_type protocol_type;
  if (af == address_family::ipv4) return resolver.resolve(protocol_type::v4(), host, service, ec);
  if (af == address_family::ipv6) {
    typedef typename Resolver::results_type results_type;
    typedef typename results_type::endpoint_type endpoint_type;
    results_type results = resolver.resolve(protocol_type::v6(), host, service, ec);
    if (ec) return results;
    // Pinning IPv6 has to mean IPv6 on the wire. macOS's getaddrinfo answers an
    // AF_INET6 query for an IPv4 literal (or an A-only name) with the v4-mapped
    // ::ffff:a.b.c.d instead of failing as glibc does, and a socket connected to
    // that talks IPv4 - so a check meant to prove "this answers over IPv6"
    // would pass over IPv4. Drop those, and report "no such host" if nothing
    // else is left, which is what the other platforms already say.
    std::vector<endpoint_type> native;
    bool dropped = false;
    for (const auto &entry : results) {
      const endpoint_type ep = entry.endpoint();
      if (ep.address().is_v6() && ep.address().to_v6().is_v4_mapped())
        dropped = true;
      else
        native.push_back(ep);
    }
    if (!dropped) return results;
    if (native.empty()) {
      ec = boost::asio::error::host_not_found;
      return results_type();
    }
    return results_type::create(native.begin(), native.end(), host, service);
  }
  return resolver.resolve(host, service, ec);
}

}  // namespace net
