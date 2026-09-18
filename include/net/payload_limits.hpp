// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

#include <str/xtos.hpp>

namespace net {
namespace payload {

// The NSCA and NRPE clients size a heap buffer from a `payload length` /
// `buffer length` value that a request may supply. Without a bound a caller
// can name a value near INT_MAX and make one submission allocate gigabytes
// (a CSPRNG buffer for NSCA, a zeroed packet for NRPE). Both protocols carry
// far smaller packets than that, and the server side already refuses such
// lengths, so the client clamps to the protocol maximum instead of trying.
//
// NSCA is 512 bytes in the reference implementation; forks raise it, but the
// packet is a fixed-size struct so nothing sane goes near 64 KiB.
constexpr unsigned int max_nsca_payload_length = 65536;
// NRPE v2 uses 1024 by default and recompiled agents use 4096 or 16384, but
// v3/v4 carry the length on the wire and this agent's own decoder accepts up
// to 1 MiB (see nrpe::packet). That ceiling is the bound to enforce here: the
// finding is that a caller could name a value near INT_MAX and get a
// multi-gigabyte allocation, not that large-but-supported payloads should
// stop working. Clamping to 64 KiB instead would have silently shrunk the
// protocol - scripts/python/test_nrpe.py exercises a 1 MiB payload end to
// end, and it is a supported configuration.
constexpr unsigned int max_nrpe_payload_length = 1024 * 1024;
// Below this the packet builders have no room for their own headers and the
// substr-based split logic underflows.
constexpr unsigned int min_payload_length = 16;

// Clamps `requested` into [min_payload_length, max_length]. When the value is
// out of range `reason` is filled in with a message naming the protocol, so
// the caller can log it once; it is left untouched when the value is fine.
inline unsigned int clamp(const int requested, const unsigned int max_length, const std::string &protocol, std::string &reason) {
  if (requested < static_cast<int>(min_payload_length)) {
    reason = protocol + " payload length " + str::xtos(requested) + " is below the minimum " + str::xtos(min_payload_length) + ", using the minimum instead.";
    return min_payload_length;
  }
  if (static_cast<unsigned int>(requested) > max_length) {
    reason = protocol + " payload length " + str::xtos(requested) + " is above the maximum " + str::xtos(max_length) + ", using the maximum instead.";
    return max_length;
  }
  return static_cast<unsigned int>(requested);
}

}  // namespace payload
}  // namespace net
