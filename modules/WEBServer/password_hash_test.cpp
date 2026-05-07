#include "password_hash.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(PasswordHash, IsHashedRecognizesPrefix) {
  EXPECT_FALSE(web_password::is_hashed(""));
  EXPECT_FALSE(web_password::is_hashed("plaintext"));
  EXPECT_FALSE(web_password::is_hashed("pbkdf2"));
  EXPECT_TRUE(web_password::is_hashed("pbkdf2-sha256$100000$aa$bb"));
}

#ifdef USE_SSL
TEST(PasswordHash, HashIsNotPlaintext) {
  const std::string h = web_password::hash_password("hunter2");
  ASSERT_FALSE(h.empty());
  EXPECT_NE(h, "hunter2");
  EXPECT_TRUE(web_password::is_hashed(h));
}

TEST(PasswordHash, HashesAreSalted) {
  // Same plaintext, two distinct hashes thanks to random salt.
  const std::string a = web_password::hash_password("hunter2");
  const std::string b = web_password::hash_password("hunter2");
  ASSERT_FALSE(a.empty());
  ASSERT_FALSE(b.empty());
  EXPECT_NE(a, b);
}

TEST(PasswordHash, VerifyAcceptsCorrectPassword) {
  const std::string h = web_password::hash_password("hunter2");
  EXPECT_TRUE(web_password::verify_password("hunter2", h));
}

TEST(PasswordHash, VerifyRejectsWrongPassword) {
  const std::string h = web_password::hash_password("hunter2");
  EXPECT_FALSE(web_password::verify_password("hunter3", h));
  EXPECT_FALSE(web_password::verify_password("", h));
}

TEST(PasswordHash, VerifyRejectsCorruptHash) {
  EXPECT_FALSE(web_password::verify_password("x", "pbkdf2-sha256$"));
  EXPECT_FALSE(web_password::verify_password("x", "pbkdf2-sha256$abc$def"));
  EXPECT_FALSE(web_password::verify_password("x", "pbkdf2-sha256$100000$ZZ$bb"));  // non-hex
  EXPECT_FALSE(web_password::verify_password("x", "pbkdf2-sha256$0$aa$bb"));        // iter <= 0
}
#endif

TEST(PasswordHash, VerifyAcceptsLegacyPlaintextStored) {
  // A legacy plaintext value on disk must still be verifiable until the
  // operator re-runs add-user to migrate it.
  EXPECT_TRUE(web_password::verify_password("hunter2", "hunter2"));
  EXPECT_FALSE(web_password::verify_password("hunter3", "hunter2"));
}
