// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscp/password_hash.hpp>

#include <gtest/gtest.h>

#include <string>

TEST(PasswordHash, IsHashedRecognizesAStoredHash) {
  EXPECT_FALSE(password_hash::is_hashed(""));
  EXPECT_FALSE(password_hash::is_hashed("plaintext"));
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2"));
  EXPECT_TRUE(password_hash::is_hashed("pbkdf2-sha256$100000$aa$bb"));
}

TEST(PasswordHash, IsHashedRejectsAPasswordThatMerelyStartsWithThePrefix) {
  // The callers of is_hashed() are the writers, and they store what it calls a
  // hash without touching it. A password is only text, so it may start with
  // the prefix; answering on the prefix alone stored such a password in the
  // clear AND left nothing able to verify it - the account was then locked out
  // by a command that reported success.
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$my-secret"));
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$"));
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$100000"));
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$100000$aa"));
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$abc$aa$bb"));        // iterations not a number
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$0$aa$bb"));          // iterations out of range
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$99999999$aa$bb"));   // ditto, and atoi-overflow bait
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$100000$ZZ$bb"));     // salt not hex
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$100000$aa$b"));      // odd-length hash
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$100000$$bb"));       // empty salt
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2-sha256$100000$aa$"));       // empty hash
}

#ifdef USE_SSL
TEST(PasswordHash, HashIsNotPlaintext) {
  const std::string h = password_hash::hash_password("hunter2");
  ASSERT_FALSE(h.empty());
  EXPECT_NE(h, "hunter2");
  EXPECT_TRUE(password_hash::is_hashed(h));
}

TEST(PasswordHash, HashesAreSalted) {
  // Same plaintext, two distinct hashes thanks to random salt.
  const std::string a = password_hash::hash_password("hunter2");
  const std::string b = password_hash::hash_password("hunter2");
  ASSERT_FALSE(a.empty());
  ASSERT_FALSE(b.empty());
  EXPECT_NE(a, b);
}

TEST(PasswordHash, VerifyAcceptsCorrectPassword) {
  const std::string h = password_hash::hash_password("hunter2");
  EXPECT_TRUE(password_hash::verify_password("hunter2", h));
}

TEST(PasswordHash, VerifyRejectsWrongPassword) {
  const std::string h = password_hash::hash_password("hunter2");
  EXPECT_FALSE(password_hash::verify_password("hunter3", h));
  EXPECT_FALSE(password_hash::verify_password("", h));
}

TEST(PasswordHash, PrefixShapedPasswordSurvivesAStoreAndLoginRoundTrip) {
  // The regression in full: `nscp web install --password pbkdf2-sha256$...`
  // stores what is_hashed() leaves alone, and the login then verifies against
  // it. The two have to agree about what a hash is, or the operator is locked
  // out of the agent they just installed.
  const std::string password = "pbkdf2-sha256$my-secret";
  ASSERT_FALSE(password_hash::is_hashed(password)) << "a password, so it gets hashed on the way in";
  const std::string stored = password_hash::hash_password(password);
  ASSERT_FALSE(stored.empty());
  EXPECT_TRUE(password_hash::is_hashed(stored));
  EXPECT_TRUE(password_hash::verify_password(password, stored));
  EXPECT_FALSE(password_hash::verify_password("pbkdf2-sha256$other", stored));
}

TEST(PasswordHash, VerifyRejectsCorruptHash) {
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$"));
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$abc$def"));
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$100000$ZZ$bb"));  // non-hex
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$0$aa$bb"));       // iter <= 0

  // Including when the corrupt value is exactly the string handed in: a
  // damaged hash never degrades into a clear-text credential of its own.
  EXPECT_FALSE(password_hash::verify_password("pbkdf2-sha256$my-secret", "pbkdf2-sha256$my-secret"));
}
#endif

TEST(PasswordHash, VerifyAcceptsLegacyPlaintextStored) {
  // A legacy plaintext value on disk must still be verifiable until the
  // operator re-runs add-user to migrate it.
  EXPECT_TRUE(password_hash::verify_password("hunter2", "hunter2"));
  EXPECT_FALSE(password_hash::verify_password("hunter3", "hunter2"));
}
