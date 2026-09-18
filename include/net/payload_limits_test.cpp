// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The clamp the NSCA and NRPE clients put on a request-supplied
// `payload length` / `buffer length`.
//
// Both sized a heap buffer straight from that value, which to_int caps only at
// INT_MAX: one submission asking for 2147483647 allocated about 2 GB - of
// CSPRNG output for NSCA, of zeroed packet for NRPE - per payload. On 32-bit
// builds that is a bad_alloc per call, on 64-bit it is memory exhaustion from a
// handful of concurrent requests. Neither protocol carries packets anywhere
// near that size and the server side already refuses such lengths.

#include <gtest/gtest.h>

#include <net/payload_limits.hpp>
#include <string>

TEST(PayloadLimits, AReasonableLengthPassesThroughUntouched) {
  std::string reason;
  EXPECT_EQ(net::payload::clamp(512, net::payload::max_nsca_payload_length, "NSCA", reason), 512u);
  EXPECT_TRUE(reason.empty()) << reason;

  EXPECT_EQ(net::payload::clamp(1024, net::payload::max_nrpe_payload_length, "NRPE", reason), 1024u);
  EXPECT_TRUE(reason.empty()) << reason;
}

TEST(PayloadLimits, TheMaximumItselfIsAllowed) {
  std::string reason;
  EXPECT_EQ(net::payload::clamp(65536, net::payload::max_nsca_payload_length, "NSCA", reason), 65536u);
  EXPECT_TRUE(reason.empty()) << reason;
}

TEST(PayloadLimits, AMebibyteNrpePayloadIsSupported) {
  // The NRPE v3/v4 decoder accepts payloads up to 1 MiB and
  // scripts/python/test_nrpe.py drives an SSL exchange at exactly that size,
  // so the clamp must not shrink it. A tighter bound here passes every unit
  // test and then fails the integration suite, which is how it was caught.
  std::string reason;
  EXPECT_EQ(net::payload::clamp(1048576, net::payload::max_nrpe_payload_length, "NRPE", reason), 1048576u);
  EXPECT_TRUE(reason.empty()) << reason;
}

TEST(PayloadLimits, AnAbsurdLengthIsClampedAndExplained) {
  std::string reason;
  EXPECT_EQ(net::payload::clamp(2147483647, net::payload::max_nsca_payload_length, "NSCA", reason), net::payload::max_nsca_payload_length);
  ASSERT_FALSE(reason.empty());
  EXPECT_NE(reason.find("NSCA"), std::string::npos) << reason;
  EXPECT_NE(reason.find("2147483647"), std::string::npos) << reason;
}

TEST(PayloadLimits, ALengthBelowTheMinimumIsRaised) {
  // The packet builders need room for their own headers, and the NRPE split
  // logic computes `payload length - 1`, which underflows to SIZE_MAX at zero.
  std::string reason;
  EXPECT_EQ(net::payload::clamp(0, net::payload::max_nrpe_payload_length, "NRPE", reason), net::payload::min_payload_length);
  EXPECT_FALSE(reason.empty());

  reason.clear();
  EXPECT_EQ(net::payload::clamp(-1, net::payload::max_nrpe_payload_length, "NRPE", reason), net::payload::min_payload_length);
  EXPECT_FALSE(reason.empty());
}

TEST(PayloadLimits, TheReasonIsLeftAloneWhenNothingIsWrong) {
  std::string reason = "untouched";
  net::payload::clamp(1024, net::payload::max_nrpe_payload_length, "NRPE", reason);
  EXPECT_EQ(reason, "untouched");
}
