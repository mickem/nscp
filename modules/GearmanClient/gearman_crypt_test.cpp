// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_crypt.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

#ifndef GEARMAN_FIXTURE_DIR
#error "GEARMAN_FIXTURE_DIR must point at modules/GearmanClient/fixtures"
#endif

using namespace gearman;

namespace {

/**
 * The payloads under `modules/GearmanClient/fixtures/` came off the wire of a
 * real Naemon with ConSol's mod_gearman and a real Nagios Core with the
 * nagios-mod-gearman fork (see the README there). They carry no trailing
 * newline, so they are read byte for byte.
 */
std::string read_fixture(const std::string &name) {
  const std::string path = std::string(GEARMAN_FIXTURE_DIR) + "/" + name;
  std::ifstream file(path.c_str(), std::ios::binary);
  EXPECT_TRUE(file.is_open()) << "missing fixture " << path;
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

/** The shared password both capture runs used. */
std::string fixture_key() { return read_fixture("key.txt"); }

}  // namespace

// ============================================================================
// Key derivation
// ============================================================================

TEST(gearman_crypt, the_password_is_nul_padded_to_32_bytes) {
  const std::string key = make_key("nscp-test-key");
  ASSERT_EQ(key_bytes, key.size());
  EXPECT_EQ("nscp-test-key", key.substr(0, 13));
  EXPECT_EQ(std::string(key_bytes - 13, '\0'), key.substr(13));
}

TEST(gearman_crypt, an_empty_password_is_32_nul_bytes) { EXPECT_EQ(std::string(key_bytes, '\0'), make_key("")); }

TEST(gearman_crypt, a_password_longer_than_32_bytes_is_truncated) {
  const std::string password(40, 'x');
  const std::string key = make_key(password);
  ASSERT_EQ(key_bytes, key.size());
  EXPECT_EQ(std::string(key_bytes, 'x'), key);
  // Which means the tail of an over-long password is not part of the secret.
  EXPECT_EQ(key, make_key(password + "and-more"));
}

TEST(gearman_crypt, a_password_of_exactly_32_bytes_is_used_whole) {
  const std::string password(key_bytes, 'k');
  EXPECT_EQ(password, make_key(password));
}

// ============================================================================
// Against the captured payloads
// ============================================================================

TEST(gearman_crypt, decrypts_what_consol_mod_gearman_sent) {
  const std::string text = decrypt_payload(read_fixture("naemon-job-service.b64"), fixture_key());
  EXPECT_EQ(
      "type=service\n"
      "result_queue=check_results\n"
      "target_queue=hostgroup_gearman-test\n"
      "host_name=nscp-test\n"
      "service_description=helper\n"
      "core_time=1789564903.687813\n"
      "timeout=3\n"
      "command_line=check_ok message=hello\n\n\n",
      text);
}

TEST(gearman_crypt, decrypts_what_the_nagios_fork_sent) {
  const std::string text = decrypt_payload(read_fixture("nagios-job-service.b64"), fixture_key());
  // The fork's NEB module adds start_time and next_check; everything else,
  // the envelope included, is identical.
  EXPECT_EQ(
      "type=service\n"
      "result_queue=check_results\n"
      "target_queue=hostgroup_gearman-test\n"
      "host_name=nscp-test\n"
      "service_description=helper\n"
      "start_time=1789564901.0\n"
      "next_check=1789564901.0\n"
      "core_time=1789564901.437880\n"
      "timeout=3\n"
      "command_line=check_ok message=hello\n\n\n",
      text);
}

TEST(gearman_crypt, re_encrypting_a_captured_payload_reproduces_it_byte_for_byte) {
  // The whole claim of the module's crypto: one routine that produces exactly
  // what both flavours produce, so a core accepts what we send.
  const char *const fixtures[] = {"naemon-job-host.b64",
                                  "naemon-job-service.b64",
                                  "naemon-result-active-service.b64",
                                  "naemon-result-passive-host.b64",
                                  "naemon-result-passive-service.b64",
                                  "nagios-job-host.b64",
                                  "nagios-job-service.b64",
                                  "nagios-result-active-service.b64",
                                  "nagios-result-passive-host.b64",
                                  "nagios-result-passive-service.b64"};
  const std::string key = fixture_key();
  for (const char *const name : fixtures) {
    const std::string captured = read_fixture(name);
    const std::string text = decrypt_payload(captured, key);
    EXPECT_EQ(captured, encrypt_payload(text, key)) << name;
  }
}

TEST(gearman_crypt, the_wrong_key_does_not_produce_a_readable_payload) {
  const std::string captured = read_fixture("naemon-job-service.b64");
  // ECB without a MAC cannot tell "wrong key" from "corrupt": decryption
  // succeeds and yields rubbish. What the caller checks is the type= prefix.
  std::string text;
  try {
    text = decrypt_payload(captured, "not-the-key");
  } catch (const crypt_error &) {
    SUCCEED();
    return;
  }
  EXPECT_NE(0, text.compare(0, 5, "type="));
}

// ============================================================================
// Padding and encoding edge cases
// ============================================================================

TEST(gearman_crypt, round_trips_text_of_every_length_around_a_block) {
  const std::string key = "a-key";
  for (std::size_t length = 1; length <= 4 * block_size; ++length) {
    const std::string text(length, 'x');
    EXPECT_EQ(text, decrypt_payload(encrypt_payload(text, key), key)) << "at " << length << " characters";
  }
}

TEST(gearman_crypt, a_plaintext_on_a_block_boundary_gets_the_reference_pad) {
  // 15 characters plus the terminating NUL fill one block exactly, and
  // BLOCKSIZE % 16 == 0 leaves it alone: one block out.
  const std::string exact(15, 'x');
  EXPECT_EQ(block_size, decode_plain_payload(encrypt_payload(exact, "k")).size());

  // 31 plus the NUL is two blocks, and the reference's BLOCKSIZE % len test
  // is true there, so it appends a whole extra block of zeros. Reproducing
  // that quirk is what keeps the bytes identical to the cores'.
  const std::string two_blocks(31, 'x');
  EXPECT_EQ(3 * block_size, decode_plain_payload(encrypt_payload(two_blocks, "k")).size());
}

TEST(gearman_crypt, base64_with_embedded_newlines_still_decodes) {
  const std::string key = fixture_key();
  const std::string captured = read_fixture("naemon-result-passive-host.b64");
  std::string wrapped;
  for (std::size_t i = 0; i < captured.size(); ++i) {
    if (i > 0 && i % 64 == 0) wrapped.push_back('\n');
    wrapped.push_back(captured[i]);
  }
  wrapped.push_back('\n');
  EXPECT_EQ(decrypt_payload(captured, key), decrypt_payload(wrapped, key));
}

TEST(gearman_crypt, a_payload_that_is_not_base64_is_refused) {
  EXPECT_THROW(decrypt_payload("not base64 at all!", "k"), crypt_error);
  EXPECT_THROW(decrypt_payload("", "k"), crypt_error);
  EXPECT_THROW(decrypt_payload("   \n  ", "k"), crypt_error);
}

TEST(gearman_crypt, a_payload_shorter_than_one_block_is_refused) {
  // Eight bytes of valid base64, half a block.
  EXPECT_THROW(decrypt_payload("QUJDREVGR0g=", "k"), crypt_error);
}

// ============================================================================
// Plain mode and the envelope wrapper
// ============================================================================

TEST(gearman_crypt, plain_mode_is_base64_and_nothing_else) {
  const std::string text = "type=passive\nhost_name=win-srv01\n";
  const std::string encoded = encode_plain_payload(text);
  EXPECT_EQ("dHlwZT1wYXNzaXZlCmhvc3RfbmFtZT13aW4tc3J2MDEK", encoded);
  EXPECT_EQ(text, decode_plain_payload(encoded));
}

TEST(gearman_crypt, the_envelope_follows_the_encryption_setting) {
  const std::string text = "type=service\nhost_name=win-srv01\n";
  const envelope encrypted(true, "a-key");
  const envelope plain(false, "a-key");

  EXPECT_EQ(text, decode_payload(encode_payload(text, encrypted), encrypted));
  EXPECT_EQ(text, decode_payload(encode_payload(text, plain), plain));
  EXPECT_NE(encode_payload(text, encrypted), encode_payload(text, plain));
}

TEST(gearman_crypt, cleartext_is_not_accepted_while_encryption_is_on) {
  // Sniffing the payload and taking a readable one as cleartext would let
  // anyone who can reach gearmand queue a job without holding the key.
  const envelope encrypted(true, "a-key");
  const std::string cleartext = encode_plain_payload("type=service\nhost_name=win-srv01\ncommand_line=check_ok\n");
  std::string text;
  try {
    text = decode_payload(cleartext, encrypted);
  } catch (const crypt_error &) {
    SUCCEED();
    return;
  }
  EXPECT_NE(0, text.compare(0, 5, "type="));
}
