/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <bytes/base64.h>

#include <cstring>
#include <string>
#include <vector>

// Helper: encode a raw buffer and return the base64 string
static std::string encode_helper(const void *src, size_t srcSize) {
  size_t needed = b64::b64_encode(src, srcSize, nullptr, 0);
  std::string result(needed, '\0');
  size_t written = b64::b64_encode(src, srcSize, &result[0], needed);
  EXPECT_EQ(written, needed);
  return result;
}

// Helper: decode a base64 string and return the raw bytes
static std::vector<unsigned char> decode_helper(const std::string &encoded) {
  size_t needed = b64::b64_decode(encoded.c_str(), encoded.size(), nullptr, 0);
  std::vector<unsigned char> result(needed);
  size_t written = b64::b64_decode(encoded.c_str(), encoded.size(), result.data(), needed);
  EXPECT_EQ(written, needed);
  return result;
}

// --- Encoding tests ---

TEST(base64_encode, empty_input) {
  size_t len = b64::b64_encode("", 0, nullptr, 0);
  EXPECT_EQ(len, 0u);
}

TEST(base64_encode, single_byte) {
  // 'A' -> "QQ=="
  const char input = 'A';
  std::string encoded = encode_helper(&input, 1);
  EXPECT_EQ(encoded, "QQ==");
}

TEST(base64_encode, two_bytes) {
  // "AB" -> "QUI="
  const char input[] = "AB";
  std::string encoded = encode_helper(input, 2);
  EXPECT_EQ(encoded, "QUI=");
}

TEST(base64_encode, three_bytes_no_padding) {
  // "ABC" -> "QUJD"
  const char input[] = "ABC";
  std::string encoded = encode_helper(input, 3);
  EXPECT_EQ(encoded, "QUJD");
}

TEST(base64_encode, well_known_vectors) {
  // RFC 4648 test vectors
  EXPECT_EQ(encode_helper("f", 1), "Zg==");
  EXPECT_EQ(encode_helper("fo", 2), "Zm8=");
  EXPECT_EQ(encode_helper("foo", 3), "Zm9v");
  EXPECT_EQ(encode_helper("foob", 4), "Zm9vYg==");
  EXPECT_EQ(encode_helper("fooba", 5), "Zm9vYmE=");
  EXPECT_EQ(encode_helper("foobar", 6), "Zm9vYmFy");
}

TEST(base64_encode, binary_data) {
  unsigned char binary[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
  std::string encoded = encode_helper(binary, sizeof(binary));
  // Decode back and compare
  auto decoded = decode_helper(encoded);
  ASSERT_EQ(decoded.size(), sizeof(binary));
  EXPECT_EQ(std::memcmp(decoded.data(), binary, sizeof(binary)), 0);
}

TEST(base64_encode, length_query_with_null_dest) {
  const char input[] = "Hello";
  size_t needed = b64::b64_encode(input, 5, nullptr, 0);
  EXPECT_EQ(needed, 8u);  // ceil(5/3)*4 = 8
}

TEST(base64_encode, dest_too_small_returns_zero) {
  const char input[] = "Hello";
  char small_buf[4];
  size_t result = b64::b64_encode(input, 5, small_buf, sizeof(small_buf));
  EXPECT_EQ(result, 0u);
}

// --- Decoding tests ---

TEST(base64_decode, empty_input) {
  size_t len = b64::b64_decode("", 0, nullptr, 0);
  EXPECT_EQ(len, 0u);
}

TEST(base64_decode, well_known_vectors) {
  auto check = [](const std::string &encoded, const std::string &expected) {
    auto decoded = decode_helper(encoded);
    std::string result(decoded.begin(), decoded.end());
    EXPECT_EQ(result, expected);
  };

  check("Zg==", "f");
  check("Zm8=", "fo");
  check("Zm9v", "foo");
  check("Zm9vYg==", "foob");
  check("Zm9vYmE=", "fooba");
  check("Zm9vYmFy", "foobar");
}

TEST(base64_decode, single_padded) {
  // "QQ==" decodes to "A"
  auto decoded = decode_helper("QQ==");
  ASSERT_EQ(decoded.size(), 1u);
  EXPECT_EQ(decoded[0], 'A');
}

TEST(base64_decode, length_query_with_null_dest) {
  const char *encoded = "QUJD";
  size_t needed = b64::b64_decode(encoded, 4, nullptr, 0);
  EXPECT_EQ(needed, 3u);
}

TEST(base64_decode, dest_too_small_returns_zero) {
  const char *encoded = "Zm9vYmFy";  // "foobar" = 6 bytes
  unsigned char small_buf[2];
  size_t result = b64::b64_decode(encoded, 8, small_buf, sizeof(small_buf));
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, invalid_length_not_multiple_of_4) {
  // srcLen must be a multiple of 4
  size_t result = b64::b64_decode("QUJ", 3, nullptr, 0);
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, invalid_char_in_main_loop_pos0) {
  // 8-char string, invalid char at position 0 of first chunk (processed in main loop since srcLen > 4)
  unsigned char buf[6];
  size_t result = b64::b64_decode("!QQQZm9v", 8, buf, sizeof(buf));
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, invalid_char_in_main_loop_pos1) {
  unsigned char buf[6];
  size_t result = b64::b64_decode("Q!QQZm9v", 8, buf, sizeof(buf));
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, invalid_char_in_main_loop_pos2) {
  unsigned char buf[6];
  size_t result = b64::b64_decode("QQ!QZm9v", 8, buf, sizeof(buf));
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, invalid_char_in_main_loop_pos3) {
  unsigned char buf[6];
  size_t result = b64::b64_decode("QQQ!Zm9v", 8, buf, sizeof(buf));
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, invalid_char_in_final_chunk) {
  // Exactly 4-char string — only the final chunk path executes
  unsigned char buf[3];
  EXPECT_EQ(b64::b64_decode("!QQQ", 4, buf, sizeof(buf)), 0u);  // pos 0
  EXPECT_EQ(b64::b64_decode("Q!QQ", 4, buf, sizeof(buf)), 0u);  // pos 1
  EXPECT_EQ(b64::b64_decode("QQ!Q", 4, buf, sizeof(buf)), 0u);  // pos 2
  EXPECT_EQ(b64::b64_decode("QQQ!", 4, buf, sizeof(buf)), 0u);  // pos 3
}

TEST(base64_decode, null_src_size_query) {
  // NULL src with non-zero srcLen returns upper-bound estimate (wholeChunks * 3)
  // without scanning for padding
  size_t result = b64::b64_decode(nullptr, 8, nullptr, 0);
  EXPECT_EQ(result, 6u);  // 2 chunks * 3 bytes
}

TEST(base64_decode, invalid_length_with_whole_chunks) {
  // srcLen=5 has wholeChunks=1 but remainder=1, should return 0
  size_t result = b64::b64_decode("QUJDQ", 5, nullptr, 0);
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, padding_at_position_1) {
  // "Q===" — '=' at position 1 of last chunk
  // total = 3 - 3 = 0, so size query returns 0
  size_t result = b64::b64_decode("Q===", 4, nullptr, 0);
  EXPECT_EQ(result, 0u);
}

TEST(base64_decode, embedded_null_in_last_chunk_size_query) {
  // Tests the '\0' == *p branch in the padding scan
  char encoded[] = {'Q', 'U', 'J', 'D', 'Q', 'Q', '\0', '\0'};
  size_t result = b64::b64_decode(encoded, 8, nullptr, 0);
  // wholeChunks=2, total=6, scan finds '\0' at position 6, total -= 2 = 4
  EXPECT_EQ(result, 4u);
}

TEST(base64_decode, embedded_null_in_last_chunk_decode) {
  // Actually decode data with embedded null bytes in last chunk
  char encoded[] = {'Q', 'U', 'J', 'D', 'Q', 'Q', '\0', '\0'};
  unsigned char buf[4];
  size_t written = b64::b64_decode(encoded, 8, buf, sizeof(buf));
  EXPECT_EQ(written, 4u);
  // First chunk "QUJD" decodes to "ABC"
  EXPECT_EQ(buf[0], 'A');
  EXPECT_EQ(buf[1], 'B');
  EXPECT_EQ(buf[2], 'C');
}

// --- Round-trip tests ---

TEST(base64_roundtrip, ascii_string) {
  const std::string original = "Hello, World!";
  std::string encoded = encode_helper(original.data(), original.size());
  auto decoded = decode_helper(encoded);
  std::string result(decoded.begin(), decoded.end());
  EXPECT_EQ(result, original);
}

TEST(base64_roundtrip, all_byte_values) {
  // Encode all 256 byte values and verify round-trip
  unsigned char all_bytes[256];
  for (int i = 0; i < 256; ++i) {
    all_bytes[i] = static_cast<unsigned char>(i);
  }
  std::string encoded = encode_helper(all_bytes, sizeof(all_bytes));
  auto decoded = decode_helper(encoded);
  ASSERT_EQ(decoded.size(), sizeof(all_bytes));
  EXPECT_EQ(std::memcmp(decoded.data(), all_bytes, sizeof(all_bytes)), 0);
}

TEST(base64_roundtrip, various_lengths) {
  // Test lengths 0..20 to exercise all padding cases
  for (size_t len = 0; len <= 20; ++len) {
    std::vector<unsigned char> input(len);
    for (size_t i = 0; i < len; ++i) {
      input[i] = static_cast<unsigned char>(i * 7 + 13);
    }
    if (len == 0) {
      size_t needed = b64::b64_encode(input.data(), 0, nullptr, 0);
      EXPECT_EQ(needed, 0u);
      continue;
    }
    std::string encoded = encode_helper(input.data(), len);
    auto decoded = decode_helper(encoded);
    ASSERT_EQ(decoded.size(), len);
    EXPECT_EQ(std::memcmp(decoded.data(), input.data(), len), 0);
  }
}

