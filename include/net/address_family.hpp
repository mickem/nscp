// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/system/error_code.hpp>
#include <string>

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
// protocol-specific asio includes.
template <typename Resolver>
typename Resolver::results_type resolve_for_family(Resolver &resolver, const address_family af, const std::string &host, const std::string &service,
                                                   boost::system::error_code &ec) {
  typedef typename Resolver::protocol_type protocol_type;
  if (af == address_family::ipv4) return resolver.resolve(protocol_type::v4(), host, service, ec);
  if (af == address_family::ipv6) return resolver.resolve(protocol_type::v6(), host, service, ec);
  return resolver.resolve(host, service, ec);
}

}  // namespace net
