// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for Mongoose::Helpers (base64 encode/decode).
 */

#include "Helpers.h"

#include <gtest/gtest.h>

#include <string>

using Mongoose::Helpers;

TEST(Helpers, Base64KnownVectors) {
  // RFC 4648 test vectors.
  EXPECT_EQ(Helpers::encode_b64(""), "");
  EXPECT_EQ(Helpers::encode_b64("f"), "Zg==");
  EXPECT_EQ(Helpers::encode_b64("fo"), "Zm8=");
  EXPECT_EQ(Helpers::encode_b64("foo"), "Zm9v");
  EXPECT_EQ(Helpers::encode_b64("foob"), "Zm9vYg==");
  EXPECT_EQ(Helpers::encode_b64("fooba"), "Zm9vYmE=");
  EXPECT_EQ(Helpers::encode_b64("foobar"), "Zm9vYmFy");
}

TEST(Helpers, Base64DecodeKnownVectors) {
  EXPECT_EQ(Helpers::decode_b64(""), "");
  EXPECT_EQ(Helpers::decode_b64("Zg=="), "f");
  EXPECT_EQ(Helpers::decode_b64("Zm9v"), "foo");
  EXPECT_EQ(Helpers::decode_b64("Zm9vYmFy"), "foobar");
}

TEST(Helpers, Base64RoundTripText) {
  const std::string original =
      "The quick brown fox jumps over the lazy dog. "
      "Sphinx of black quartz, judge my vow.";
  EXPECT_EQ(Helpers::decode_b64(Helpers::encode_b64(original)), original);
}

TEST(Helpers, Base64RoundTripBinary) {
  // The full 0..255 byte range survives a round-trip — including embedded
  // NULs, which the previous mongoose-backed implementation truncated on
  // (it strncpy'd the input).
  std::string original;
  for (int i = 0; i < 256; ++i) {
    original.push_back(static_cast<char>(i));
  }
  EXPECT_EQ(Helpers::decode_b64(Helpers::encode_b64(original)), original);
}
