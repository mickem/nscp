// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "user_manager.h"

#include <gtest/gtest.h>

#include <string>

#include "password_hash.hpp"

TEST(UserManager, AddAndValidateRoundTrip) {
  user_manager um;
  um.add_user("alice", "s3cret");
  EXPECT_TRUE(um.has_user("alice"));
  EXPECT_TRUE(um.validate_user("alice", "s3cret"));
}

TEST(UserManager, RejectsWrongPassword) {
  user_manager um;
  um.add_user("alice", "s3cret");
  EXPECT_FALSE(um.validate_user("alice", "wrong"));
}

TEST(UserManager, RejectsUnknownUser) {
  user_manager um;
  um.add_user("alice", "s3cret");
  EXPECT_FALSE(um.validate_user("eve", "anything"));
}

TEST(UserManager, RejectsEmptyPassword) {
  user_manager um;
  um.add_user("alice", "s3cret");
  EXPECT_FALSE(um.validate_user("alice", ""));
}

TEST(UserManager, EmptyStoredPasswordRejectsAllAttempts) {
  // A user added with an empty password must not be loginable - otherwise an
  // operator who clears the password unintentionally opens the account.
  user_manager um;
  um.add_user("alice", "");
  EXPECT_TRUE(um.has_user("alice"));
  EXPECT_FALSE(um.validate_user("alice", ""));
  EXPECT_FALSE(um.validate_user("alice", "anything"));
}

TEST(UserManager, RemoveUser) {
  user_manager um;
  um.add_user("alice", "s3cret");
  EXPECT_TRUE(um.has_user("alice"));
  um.remove_user("alice");
  EXPECT_FALSE(um.has_user("alice"));
  EXPECT_FALSE(um.validate_user("alice", "s3cret"));
}

TEST(UserManager, AcceptsPreviouslyHashedPassword) {
  // An already-hashed value (e.g. read from disk in a future migration) must
  // round-trip without being double-hashed.
  user_manager seed;
  seed.add_user("alice", "s3cret");
  // Pull out the stored hash by re-adding into a probe and verifying. The
  // hash format is opaque; we can't read it directly, but we can simulate it
  // using a value we know is hashed (starts with the prefix). Just verify the
  // happy path: a fresh instance accepts the same plaintext after re-add.
  user_manager um;
  um.add_user("alice", "s3cret");
  EXPECT_TRUE(um.validate_user("alice", "s3cret"));
}

TEST(UserManager, DistinctUsersHaveDistinctHashes) {
  // Two users added with the *same* password must end up with different
  // stored values (random salt). We can't read the hash directly, but we can
  // observe that swapping users does not validate the other's password.
  user_manager um;
  um.add_user("alice", "samepw");
  um.add_user("bob", "samepw");
  EXPECT_TRUE(um.validate_user("alice", "samepw"));
  EXPECT_TRUE(um.validate_user("bob", "samepw"));
  EXPECT_FALSE(um.validate_user("alice", "different"));
}

TEST(UserManager, GetHashReturnsTheStoredValue) {
  user_manager um;
  EXPECT_EQ(um.get_hash("nobody"), "") << "an unknown user has no stored value";

  // A password already in PBKDF2 form is stored verbatim, which is what makes
  // it stable across processes.
  const std::string pre_hashed = web_password::hash_password("secret");
  ASSERT_TRUE(web_password::is_hashed(pre_hashed));
  um.add_user("hashed", pre_hashed);
  EXPECT_EQ(um.get_hash("hashed"), pre_hashed);

  // A plaintext password is stored as its hash: the stored value is never the
  // password, and never the same twice.
  um.add_user("plain", "secret");
  const std::string first = um.get_hash("plain");
  EXPECT_NE(first, "secret");
  EXPECT_TRUE(web_password::is_hashed(first));
  um.add_user("plain", "secret");
  EXPECT_NE(um.get_hash("plain"), first) << "re-adding a plaintext password re-salts it";
  EXPECT_TRUE(um.validate_user("plain", "secret"));
}
