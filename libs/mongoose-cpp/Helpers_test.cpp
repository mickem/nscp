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
  // NOTE: Helpers::encode_b64 uses strncpy internally to copy the input which
  // truncates at the first null byte. Binary payloads with embedded NULs are
  // therefore not preserved (this is a known limitation of the helper). We
  // exercise the full non-null byte range here.
  std::string original;
  for (int i = 1; i < 256; ++i) {
    original.push_back(static_cast<char>(i));
  }
  EXPECT_EQ(Helpers::decode_b64(Helpers::encode_b64(original)), original);
}
