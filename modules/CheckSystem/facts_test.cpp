// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the parts of the fact gatherers that are pure: the two
// mappings that have to produce exactly what the Unix module produces, because
// a fleet query over `state` or a server-side join on `mac` is meaningless if
// the two platforms spell them differently.
//
// The Win32 gatherers themselves need a machine to read, which the Windows
// integration suite provides.

#include "facts.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace check_system_facts;

// ============================================================================
// Link state
// ============================================================================

TEST(CheckSystemFactsState, UpAndDownUseTheSameWordsAsTheUnixModule) {
  EXPECT_EQ("up", operational_status_to_state(1));    // IfOperStatusUp
  EXPECT_EQ("down", operational_status_to_state(2));  // IfOperStatusDown
}

// `testing`, `dormant` and everything else Windows can report have no Linux
// counterpart; calling them `up` or `down` would make a fleet query lie.
TEST(CheckSystemFactsState, AnythingThatIsNotPlainlyUpOrDownIsUnknown) {
  EXPECT_EQ("unknown", operational_status_to_state(3));  // testing
  EXPECT_EQ("unknown", operational_status_to_state(4));  // unknown
  EXPECT_EQ("unknown", operational_status_to_state(5));  // dormant
  EXPECT_EQ("unknown", operational_status_to_state(99));
}

// Not present and lower-layer-down are down from an operator's point of view,
// and Linux reports exactly `down` for both.
TEST(CheckSystemFactsState, NotPresentAndLowerLayerDownAreDown) {
  EXPECT_EQ("down", operational_status_to_state(6));
  EXPECT_EQ("down", operational_status_to_state(7));
}

// ============================================================================
// MAC addresses
// ============================================================================

TEST(CheckSystemFactsMac, IsLowerCaseColonSeparatedAsOnLinux) {
  const unsigned char address[6] = {0x00, 0x50, 0x56, 0xAA, 0xBB, 0xCC};
  EXPECT_EQ("00:50:56:aa:bb:cc", format_mac(address, 6));
}

TEST(CheckSystemFactsMac, PadsEachByteToTwoDigits) {
  const unsigned char address[6] = {0x02, 0x0F, 0x00, 0x00, 0x00, 0x01};
  EXPECT_EQ("02:0f:00:00:00:01", format_mac(address, 6));
}

// A loopback or tunnel adapter has no hardware address; an empty string is
// omitted by the builder, which is what "this adapter has none" should look
// like rather than "00:00:...".
TEST(CheckSystemFactsMac, AnAdapterWithoutAHardwareAddressReportsNothing) {
  const unsigned char address[1] = {0};
  EXPECT_EQ("", format_mac(address, 0));
  EXPECT_EQ("", format_mac(nullptr, 6));
}

// InfiniBand adapters have 20-byte addresses; the formatter must not assume
// six.
TEST(CheckSystemFactsMac, HandlesAnAddressLongerThanSixBytes) {
  const unsigned char address[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  EXPECT_EQ("00:11:22:33:44:55:66:77", format_mac(address, 8));
}

// ============================================================================
// Address formatting
// ============================================================================
//
// These are written here rather than called from the platform because
// InetNtop is Vista-or-later and this agent still builds for XP. The values
// below are what inet_ntop produces for the same bytes on the Unix side, so
// the same address reported by a Windows host and a Linux host is the same
// string and the server can join on it.

namespace {
// An IPv6 address from its eight groups, most significant first.
std::vector<unsigned char> v6(unsigned int g0, unsigned int g1, unsigned int g2, unsigned int g3, unsigned int g4, unsigned int g5, unsigned int g6,
                              unsigned int g7) {
  const unsigned int groups[8] = {g0, g1, g2, g3, g4, g5, g6, g7};
  std::vector<unsigned char> bytes(16);
  for (int i = 0; i < 8; ++i) {
    bytes[i * 2] = static_cast<unsigned char>((groups[i] >> 8) & 0xff);
    bytes[i * 2 + 1] = static_cast<unsigned char>(groups[i] & 0xff);
  }
  return bytes;
}
}  // namespace

TEST(CheckSystemFactsAddress, Ipv4IsADottedQuad) {
  const unsigned char address[4] = {192, 0, 2, 1};
  EXPECT_EQ("192.0.2.1", format_ipv4(address));
  const unsigned char zero[4] = {0, 0, 0, 0};
  EXPECT_EQ("0.0.0.0", format_ipv4(zero));
  const unsigned char broadcast[4] = {255, 255, 255, 255};
  EXPECT_EQ("255.255.255.255", format_ipv4(broadcast));
}

TEST(CheckSystemFactsAddress, Ipv6IsLowerCaseHexWithoutLeadingZeros) {
  EXPECT_EQ("2001:db8:85a3::8a2e:370:7334", format_ipv6(&v6(0x2001, 0x0db8, 0x85a3, 0, 0, 0x8a2e, 0x0370, 0x7334)[0]));
  EXPECT_EQ("1:2:3:4:5:6:7:8", format_ipv6(&v6(1, 2, 3, 4, 5, 6, 7, 8)[0]));
}

// RFC 5952: the longest run of two or more zero groups is compressed, once.
TEST(CheckSystemFactsAddress, Ipv6CompressesTheLongestRunOfZeroGroups) {
  EXPECT_EQ("::", format_ipv6(&v6(0, 0, 0, 0, 0, 0, 0, 0)[0]));
  EXPECT_EQ("::1", format_ipv6(&v6(0, 0, 0, 0, 0, 0, 0, 1)[0]));
  EXPECT_EQ("2001:db8::1", format_ipv6(&v6(0x2001, 0x0db8, 0, 0, 0, 0, 0, 1)[0]));
  EXPECT_EQ("2001:db8::", format_ipv6(&v6(0x2001, 0x0db8, 0, 0, 0, 0, 0, 0)[0]));
  EXPECT_EQ("1::8", format_ipv6(&v6(1, 0, 0, 0, 0, 0, 0, 8)[0]));
  EXPECT_EQ("2001:db8::1:0:0:1", format_ipv6(&v6(0x2001, 0x0db8, 0, 0, 1, 0, 0, 1)[0]));
}

// A single zero group is written as `0`: compressing it would be shorter but
// not what RFC 5952 says, nor what inet_ntop does.
TEST(CheckSystemFactsAddress, Ipv6DoesNotCompressASingleZeroGroup) {
  EXPECT_EQ("1:0:3:4:5:6:7:8", format_ipv6(&v6(1, 0, 3, 4, 5, 6, 7, 8)[0]));
}

TEST(CheckSystemFactsAddress, Ipv6WritesAnEmbeddedIpv4AddressAsADottedQuad) {
  // ::ffff:192.0.2.128 - an IPv4-mapped address.
  EXPECT_EQ("::ffff:192.0.2.128", format_ipv6(&v6(0, 0, 0, 0, 0, 0xffff, 0xc000, 0x0280)[0]));
  // ::192.0.2.128 - an IPv4-compatible address.
  EXPECT_EQ("::192.0.2.128", format_ipv6(&v6(0, 0, 0, 0, 0, 0, 0xc000, 0x0280)[0]));
  // ::1 is the loopback, not an embedded 0.0.0.1.
  EXPECT_EQ("::1", format_ipv6(&v6(0, 0, 0, 0, 0, 0, 0, 1)[0]));
}
