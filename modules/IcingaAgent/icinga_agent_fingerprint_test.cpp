// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_fingerprint.hpp"

#include <gtest/gtest.h>

using namespace icinga_agent;

namespace {
const std::string kHex = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
}

TEST(IcingaAgentFingerprint, AcceptsTheFormsOperatorsPaste) {
  std::string out;
  ASSERT_TRUE(normalize_fingerprint(kHex, out));
  EXPECT_EQ(out, kHex);
  // `openssl x509 -noout -fingerprint -sha256`
  ASSERT_TRUE(normalize_fingerprint("SHA256 Fingerprint=01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF", out));
  EXPECT_EQ(out, kHex);
  // Node's X509Certificate.fingerprint256, PowerShell's grouping
  ASSERT_TRUE(normalize_fingerprint("01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF", out));
  ASSERT_TRUE(normalize_fingerprint("0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF", out));
  EXPECT_EQ(out, kHex);
}

TEST(IcingaAgentFingerprint, RejectsAnythingElse) {
  std::string out = "unchanged";
  EXPECT_FALSE(normalize_fingerprint("01:23:45:67:89:ab:cd:ef:01:23:45:67:89:ab:cd:ef:01:23:45:67", out));  // SHA-1
  EXPECT_FALSE(normalize_fingerprint(kHex + "00", out));                                                    // too long
  EXPECT_FALSE(normalize_fingerprint(kHex.substr(0, 63) + "g", out));                                       // not hex
  EXPECT_FALSE(normalize_fingerprint("", out));
  EXPECT_EQ(out, "unchanged");
}
