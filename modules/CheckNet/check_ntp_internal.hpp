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
 * Small pure helpers used by check_ntp_offset; broken out so they can be
 * unit tested without dragging in Boost.Asio.
 */

#pragma once

#include <cstdint>

namespace check_net {
namespace check_ntp_internal {

// NTP epoch (1900-01-01) is 2208988800 seconds before the unix epoch.
constexpr std::uint64_t kNtpUnixDelta = 2208988800ULL;

// Convert an NTP timestamp (uint32 seconds, uint32 fraction) read from the wire
// to milliseconds since the unix epoch. Returns 0 when both halves are zero
// (the NTP "no timestamp" sentinel) or when the seconds field is older than
// the unix epoch (1970).
inline long long ntp_to_unix_ms(std::uint32_t secs, std::uint32_t frac) {
  if (secs == 0 && frac == 0) return 0;
  if (static_cast<std::uint64_t>(secs) < kNtpUnixDelta) return 0;
  const std::uint64_t s = static_cast<std::uint64_t>(secs) - kNtpUnixDelta;
  // Fractional part is in units of 2^-32 seconds.
  const long long frac_ms = static_cast<long long>((static_cast<std::uint64_t>(frac) * 1000ULL) >> 32);
  return static_cast<long long>(s) * 1000LL + frac_ms;
}

// Compute the standard NTP offset (server - local) in milliseconds.
// t1 = local send time, t2 = server receive time, t3 = server transmit time, t4 = local receive time.
inline long long ntp_offset_ms(long long t1, long long t2, long long t3, long long t4) { return ((t2 - t1) + (t3 - t4)) / 2; }

}  // namespace check_ntp_internal
}  // namespace check_net
