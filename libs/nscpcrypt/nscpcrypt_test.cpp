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

// ---------------------------------------------------------------------------
// engine: encrypting and decrypting a payload
// ---------------------------------------------------------------------------

namespace {
constexpr int kNone = 0;
constexpr int k3Des = 3;

std::string iv_0_to_127() {
  std::string iv;
  for (int i = 0; i < 128; ++i) iv.push_back(static_cast<char>(i));
  return iv;
}

std::string hex(const std::string &s) {
  static const char *digits = "0123456789abcdef";
  std::string out;
  for (const unsigned char c : s) {
    out.push_back(digits[c >> 4]);
    out.push_back(digits[c & 0xf]);
  }
  return out;
}

const std::string kPayload = "host\tservice\t2\tdisk is full";
}  // namespace

// Every method the build offers round-trips through two engines set up the
// way an NSCA client and server are: the same password and the IV the
// server sent.
TEST(NscpCryptEngine, EveryAvailableMethodRoundTrips) {
  const std::string iv = engine::generate_transmitted_iv();
  int tried = 0;
  for (int method = 0; method < 32; ++method) {
    if (!engine::hasEncryption(method)) continue;
    ++tried;
    engine client, server;
    client.encrypt_init("secret", method, iv);
    server.encrypt_init("secret", method, iv);
    std::string buffer = kPayload;
    client.encrypt_buffer(buffer);
    if (method != kNone) EXPECT_NE(buffer, kPayload) << helpers::encryption_to_string(method);
    server.decrypt_buffer(buffer);
    EXPECT_EQ(buffer, kPayload) << helpers::encryption_to_string(method);
    EXPECT_NE(client.to_string(), "<NULL>");
  }
  EXPECT_GE(tried, 2);  // none and xor are always there
}

TEST(NscpCryptEngine, WrongPasswordDoesNotDecrypt) {
  const std::string iv = iv_0_to_127();
  for (const int method : {kXor, kAes256}) {
    if (!engine::hasEncryption(method)) continue;
    engine client, server;
    client.encrypt_init("secret", method, iv);
    server.encrypt_init("wrong", method, iv);
    std::string buffer = kPayload;
    client.encrypt_buffer(buffer);
    server.decrypt_buffer(buffer);
    EXPECT_NE(buffer, kPayload) << method;
  }
}

// XOR is the NSCA obfuscation: each byte against the IV and the password,
// both rotating.
TEST(NscpCryptEngine, XorKnownAnswer) {
  engine e;
  e.encrypt_init("ab", kXor, std::string("\x01\x02\x03", 3));
  std::string buffer = "xyzw";
  e.encrypt_buffer(buffer);
  const std::string expected = {static_cast<char>('x' ^ 0x01 ^ 'a'), static_cast<char>('y' ^ 0x02 ^ 'b'), static_cast<char>('z' ^ 0x03 ^ 'a'),
                                static_cast<char>('w' ^ 0x01 ^ 'b')};
  EXPECT_EQ(buffer, expected);
}

// Known answers from an independent implementation (Python `cryptography`,
// CFB8, the password zero-padded to the key size, the IV's first block).
// This is what keeps NSClient++ readable by a real NSCA daemon, which uses
// mcrypt's byte-at-a-time CFB the same way.
TEST(NscpCryptEngine, CfbKnownAnswers) {
  if (!engine::hasEncryption(kAes256)) GTEST_SKIP() << "built without crypto++";
  const struct {
    int method;
    const char *hex;
  } cases[] = {
      {kAes128, "bdb34d3e05504b515a7adf4d6058c43a67162359a1fcc5c03c1eef"},
      {kAes256, "21d6bbd6a81f3f9b4a9049a51bc6dc6da2b4156c3f4bab368d3e44"},
      {k3Des, "b662aba1ff97bf88093f423d4b8cae61c1553b4cc76cd827fa451d"},
  };
  for (const auto &c : cases) {
    engine e;
    e.encrypt_init("secret", c.method, iv_0_to_127());
    std::string buffer = kPayload;
    e.encrypt_buffer(buffer);
    EXPECT_EQ(hex(buffer), c.hex) << helpers::encryption_to_string(c.method);
  }
}

// The cipher keeps its state across calls, as the stream does: encrypting a
// packet in two pieces gives the same bytes as in one.
TEST(NscpCryptEngine, CfbStateCarriesAcrossCalls) {
  if (!engine::hasEncryption(kAes256)) GTEST_SKIP() << "built without crypto++";
  engine whole, pieces;
  whole.encrypt_init("secret", kAes256, iv_0_to_127());
  pieces.encrypt_init("secret", kAes256, iv_0_to_127());
  std::string all = kPayload;
  whole.encrypt_buffer(all);
  std::string a = kPayload.substr(0, 10), b = kPayload.substr(10);
  pieces.encrypt_buffer(a);
  pieces.encrypt_buffer(b);
  EXPECT_EQ(a + b, all);
}

TEST(NscpCryptEngine, IvShorterThanTheBlockIsRefused) {
  if (!engine::hasEncryption(kAes256)) GTEST_SKIP() << "built without crypto++";
  engine e;
  EXPECT_THROW(e.encrypt_init("secret", kAes256, std::string(8, 'x')), encryption_exception);
}

TEST(NscpCryptEngine, UnusableEngineThrows) {
  engine e;
  std::string buffer = "x";
  EXPECT_EQ(e.to_string(), "<NULL>");
  EXPECT_THROW(e.encrypt_buffer(buffer), encryption_exception);
  EXPECT_THROW(e.decrypt_buffer(buffer), encryption_exception);
  EXPECT_THROW(e.encrypt_init("secret", 99, ""), encryption_exception);
  EXPECT_FALSE(engine::hasEncryption(99));
  EXPECT_EQ(engine::get_encryption_core(99), nullptr);
}

// The server generates the IV when it receives none; the empty buffers must
// still survive.
TEST(NscpCryptEngine, ServerSideInitAndEmptyBuffers) {
  engine e;
  e.encrypt_init("secret", kXor, "");
  std::string empty;
  e.encrypt_buffer(empty);
  e.decrypt_buffer(empty);
  EXPECT_TRUE(empty.empty());
}

TEST(NscpCryptEngine, RandomBuffers) {
  EXPECT_EQ(engine::generate_transmitted_iv().size(), 128u);
  EXPECT_EQ(engine::generate_transmitted_iv(16).size(), 16u);
  EXPECT_TRUE(engine::generate_transmitted_iv(0).empty());
  EXPECT_NE(engine::generate_transmitted_iv(), engine::generate_transmitted_iv());
  engine e;
  EXPECT_EQ(e.get_rand_buffer(32).size(), 32u);
}

TEST(NscpCryptHelpers, CryptoStringListsTheAvailableMethods) {
  const std::string s = helpers::get_crypto_string();
  EXPECT_NE(s.find("XOR"), std::string::npos) << s;
  if (engine::hasEncryption(kAes256)) EXPECT_NE(s.find("AES"), std::string::npos) << s;
}
