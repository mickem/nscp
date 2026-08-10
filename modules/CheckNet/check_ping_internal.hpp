// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace check_net {
namespace check_ping_internal {

// Largest ICMP payload that still fits an IPv4 datagram: 65535 total, minus the
// 20 byte IP header and the 8 byte ICMP header.
constexpr int kMaxPingPayload = 65507;

// Build the ICMP payload for a requested size.
//
// `size` of 0 means "no size was asked for" and hands back the payload
// unchanged, so the default behaviour is untouched. Otherwise the payload is
// repeated and then cut to exactly `size` bytes, the way ping fills its packets
// with a repeating pattern - which keeps the bytes on the wire recognisable
// instead of a run of zeroes.
//
// An empty payload would make the fill loop spin forever, so it is substituted
// with a single character first.
inline std::string build_ping_payload(const std::string &payload, const int size) {
  if (size <= 0) return payload;

  const std::string pattern = payload.empty() ? std::string("x") : payload;
  std::string filled;
  filled.reserve(static_cast<std::size_t>(size));
  while (filled.size() < static_cast<std::size_t>(size)) filled += pattern;
  filled.resize(static_cast<std::size_t>(size));
  return filled;
}

}  // namespace check_ping_internal
}  // namespace check_net
