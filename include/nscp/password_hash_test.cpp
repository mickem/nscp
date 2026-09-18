// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscp/password_hash.hpp>

#include <gtest/gtest.h>

#include <string>

TEST(PasswordHash, IsHashedRecognizesPrefix) {
  EXPECT_FALSE(password_hash::is_hashed(""));
  EXPECT_FALSE(password_hash::is_hashed("plaintext"));
  EXPECT_FALSE(password_hash::is_hashed("pbkdf2"));
  EXPECT_TRUE(password_hash::is_hashed("pbkdf2-sha256$100000$aa$bb"));
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

TEST(PasswordHash, VerifyRejectsCorruptHash) {
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$"));
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$abc$def"));
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$100000$ZZ$bb"));  // non-hex
  EXPECT_FALSE(password_hash::verify_password("x", "pbkdf2-sha256$0$aa$bb"));       // iter <= 0
}
#endif

TEST(PasswordHash, VerifyAcceptsLegacyPlaintextStored) {
  // A legacy plaintext value on disk must still be verifiable until the
  // operator re-runs add-user to migrate it.
  EXPECT_TRUE(password_hash::verify_password("hunter2", "hunter2"));
  EXPECT_FALSE(password_hash::verify_password("hunter3", "hunter2"));
}
