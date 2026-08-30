// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscpcrypt/nscpcrypt.hpp>

using nscp::encryption::encryption_exception;
using nscp::encryption::engine;
using nscp::encryption::helpers;

namespace {
// Wire ids from nscpcrypt.cpp (the legacy NSCA numbering).
constexpr int kXor = 1;
constexpr int kAes128 = 14;
constexpr int kAes256 = 16;
}  // namespace

TEST(NscpCryptHelpers, NoneAndEmptySelectNoEncryption) {
  EXPECT_EQ(helpers::no_encryption, helpers::encryption_to_int(""));
  EXPECT_EQ(helpers::no_encryption, helpers::encryption_to_int("none"));
  EXPECT_EQ(helpers::no_encryption, helpers::encryption_to_int("NONE"));
  EXPECT_EQ(helpers::no_encryption, helpers::encryption_to_int(" none "));
  EXPECT_EQ(helpers::no_encryption, helpers::encryption_to_int("0"));
}

TEST(NscpCryptHelpers, KnownNamesResolve) {
  EXPECT_EQ(kXor, helpers::encryption_to_int("xor"));
  EXPECT_EQ(kXor, helpers::encryption_to_int("XOR"));
  EXPECT_EQ(kXor, helpers::encryption_to_int("1"));
  if (engine::hasEncryption(kAes256)) {
    // Built with crypto++: the aes aliases resolve to rijndael-256.
    EXPECT_EQ(kAes256, helpers::encryption_to_int("aes"));
    EXPECT_EQ(kAes256, helpers::encryption_to_int("aes256"));
    EXPECT_EQ(kAes256, helpers::encryption_to_int("rijndael256"));
    EXPECT_EQ(kAes128, helpers::encryption_to_int("aes128"));
  } else {
    // Built without crypto++: an unavailable algorithm must be a hard
    // error, never a silent fall-back to plaintext.
    EXPECT_THROW(helpers::encryption_to_int("aes256"), encryption_exception);
  }
}

TEST(NscpCryptHelpers, UnknownNamesThrowInsteadOfSilentPlaintext) {
  EXPECT_THROW(helpers::encryption_to_int("aes-256"), encryption_exception);
  EXPECT_THROW(helpers::encryption_to_int("bogus"), encryption_exception);
  EXPECT_THROW(helpers::encryption_to_int("aes 256"), encryption_exception);
  EXPECT_THROW(helpers::encryption_to_int("99"), encryption_exception);
}

TEST(NscpCryptHelpers, ErrorMessageStripsControlCharacters) {
  try {
    helpers::encryption_to_int("bad\nvalue\x1b");
    FAIL() << "expected encryption_exception";
  } catch (const encryption_exception &e) {
    const std::string msg = e.what();
    EXPECT_EQ(msg.find('\n'), std::string::npos) << msg;
    EXPECT_EQ(msg.find('\x1b'), std::string::npos) << msg;
    EXPECT_NE(msg.find("badvalue"), std::string::npos) << msg;
  }
}

TEST(NscpCryptHelpers, RoundTripThroughEncryptionToString) {
  EXPECT_EQ(kXor, helpers::encryption_to_int(helpers::encryption_to_string(kXor)));
  EXPECT_EQ(helpers::no_encryption, helpers::encryption_to_int(helpers::encryption_to_string(helpers::no_encryption)));
}
