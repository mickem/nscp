// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithms/sha256.hpp>
#include <gtest/gtest.h>

#include <string>

// The digest is the fleet server's only cheap view of a host's inventory, so
// these pin it against the published FIPS 180-4 vectors rather than against
// whatever this implementation happens to produce: a value that drifts from
// what every other SHA-256 computes would make the agent and the server
// disagree about whether the document changed.

TEST(sha256, the_empty_string_hashes_to_the_published_digest) {
  EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", algorithms::sha256_hex(""));
}

TEST(sha256, abc_hashes_to_the_published_digest) {
  EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", algorithms::sha256_hex("abc"));
}

// 56 bytes: the remainder leaves no room for the length field, so the padding
// spills into a second block. This is the case a hand-rolled padding loop gets
// wrong.
TEST(sha256, a_message_that_spills_the_padding_into_a_second_block_hashes_correctly) {
  EXPECT_EQ("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
            algorithms::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
}

TEST(sha256, a_message_longer_than_one_block_hashes_correctly) {
  EXPECT_EQ("cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
            algorithms::sha256_hex("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"));
}

TEST(sha256, a_million_a_characters_hash_to_the_published_digest) {
  EXPECT_EQ("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", algorithms::sha256_hex(std::string(1000000, 'a')));
}

TEST(sha256, embedded_nul_bytes_are_hashed_rather_than_terminating_the_input) {
  const std::string with_nul("a\0b", 3);
  EXPECT_NE(algorithms::sha256_hex("a"), algorithms::sha256_hex(with_nul));
  EXPECT_EQ(64u, algorithms::sha256_hex(with_nul).size());
}

TEST(sha256, the_raw_digest_is_thirty_two_bytes) { EXPECT_EQ(32u, algorithms::sha256_raw("abc").size()); }

TEST(sha256, the_hex_digest_is_lower_case) {
  const std::string hex = algorithms::sha256_hex("NSClient++");
  for (const char c : hex) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << "unexpected character in digest: " << c;
  }
}
