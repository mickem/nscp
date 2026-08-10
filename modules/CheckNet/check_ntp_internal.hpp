// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

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

// Convert an "NTP short" fixed-point value (16 bits of seconds, 16 bits of
// fraction) to milliseconds. This is the encoding of the root delay and root
// dispersion fields of the packet header.
inline long long ntp_short_to_ms(std::uint32_t value) { return (static_cast<long long>(value) * 1000LL) >> 16; }

// RMS jitter over a series of offset measurements, in milliseconds: the root
// mean square of the differences between successive samples. This is the
// definition NTP tooling reports, so the number is directly comparable with
// what `ntpq -p` shows for the same server.
//
// Returns -1 for fewer than two samples: jitter is a property of the variation
// between measurements and is simply not defined for a single one, and -1 can
// never be confused with a real value (jitter is a magnitude, so never
// negative).
inline long long rms_jitter_ms(const std::vector<long long> &offsets) {
  if (offsets.size() < 2) return -1;
  double sum_sq = 0.0;
  for (std::size_t i = 1; i < offsets.size(); ++i) {
    const double delta = static_cast<double>(offsets[i] - offsets[i - 1]);
    sum_sq += delta * delta;
  }
  return static_cast<long long>(std::sqrt(sum_sq / static_cast<double>(offsets.size() - 1)) + 0.5);
}

}  // namespace check_ntp_internal
}  // namespace check_net
