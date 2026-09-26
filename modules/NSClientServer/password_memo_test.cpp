// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "password_memo.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {
const std::string kPassword = "check_nt-secret";
}  // namespace

TEST(CheckNtPasswordMemo, ClearTextStoredNeverDerives) {
  check_nt_password::memo m;
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(m.verify(kPassword, kPassword));
    EXPECT_FALSE(m.verify("wrong", kPassword));
  }
  EXPECT_EQ(m.derivations(), 0u) << "a clear-text value on disk is a compare, nothing more";
}

#ifdef USE_SSL
TEST(CheckNtPasswordMemo, HashedStoredDerivesOnceForManyRequests) {
  const std::string stored = password_hash::hash_password(kPassword);
  ASSERT_FALSE(stored.empty());

  check_nt_password::memo m;
  EXPECT_TRUE(m.verify(kPassword, stored));
  EXPECT_EQ(m.derivations(), 1u);

  // This is the whole point: a polled check_nt server must not re-run PBKDF2
  // per request, and a wrong password must not either - otherwise any host
  // inside `allowed hosts` can spend the agent's CPU at will.
  for (int i = 0; i < 20; ++i) {
    EXPECT_TRUE(m.verify(kPassword, stored));
    EXPECT_FALSE(m.verify("wrong", stored));
    EXPECT_FALSE(m.verify("", stored));
  }
  EXPECT_EQ(m.derivations(), 1u);
}

TEST(CheckNtPasswordMemo, AWrongPasswordDoesNotArmTheMemo) {
  const std::string stored = password_hash::hash_password(kPassword);
  ASSERT_FALSE(stored.empty());

  check_nt_password::memo m;
  EXPECT_FALSE(m.verify("wrong", stored));
  EXPECT_FALSE(m.verify("wrong", stored));
  EXPECT_EQ(m.derivations(), 2u) << "nothing is memoised until a password has actually verified";
  EXPECT_TRUE(m.verify(kPassword, stored)) << "and the right one still gets in";
}

TEST(CheckNtPasswordMemo, ANewStoredValueIsNotAnsweredFromTheOldMemo) {
  const std::string first = password_hash::hash_password("first");
  const std::string second = password_hash::hash_password("second");
  ASSERT_FALSE(first.empty());
  ASSERT_FALSE(second.empty());

  check_nt_password::memo m;
  ASSERT_TRUE(m.verify("first", first));
  // A settings reload rewrote the password. The memo keeps the stored value it
  // learned from, so the old clear text stops working even without forget().
  EXPECT_FALSE(m.verify("first", second));
  EXPECT_TRUE(m.verify("second", second));
  EXPECT_FALSE(m.verify("first", second));
}

TEST(CheckNtPasswordMemo, ForgetMakesTheNextRequestDeriveAgain) {
  const std::string stored = password_hash::hash_password(kPassword);
  ASSERT_FALSE(stored.empty());

  check_nt_password::memo m;
  ASSERT_TRUE(m.verify(kPassword, stored));
  ASSERT_EQ(m.derivations(), 1u);
  m.forget();
  EXPECT_TRUE(m.verify(kPassword, stored));
  EXPECT_EQ(m.derivations(), 2u);
}

TEST(CheckNtPasswordMemo, AnEmptyPasswordStillMemoises) {
  // Degenerate, and isPasswordOk refuses an empty *stored* value before it gets
  // here, but a hash of "" is a real stored value and must not defeat the memo.
  const std::string stored = password_hash::hash_password("");
  ASSERT_FALSE(stored.empty());

  check_nt_password::memo m;
  EXPECT_TRUE(m.verify("", stored));
  EXPECT_TRUE(m.verify("", stored));
  EXPECT_FALSE(m.verify("x", stored));
  EXPECT_EQ(m.derivations(), 1u);
}

TEST(CheckNtPasswordMemo, ADamagedHashIsRejectedAndNeverMemoised) {
  check_nt_password::memo m;
  const std::string damaged = "pbkdf2-sha256$100000$ZZ$bb";
  EXPECT_FALSE(m.verify(damaged, damaged));
  EXPECT_FALSE(m.verify("anything", damaged));
}
#endif
