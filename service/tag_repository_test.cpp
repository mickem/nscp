// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include "tag_repository.hpp"

using set_result = nsclient::core::tag_repository::set_result;

TEST(TagRepository, StartsEmptyAtRevisionZero) {
  const nsclient::core::tag_repository repo;
  EXPECT_TRUE(repo.get_all().empty());
  EXPECT_EQ(repo.get_revision(), 0u);
}

TEST(TagRepository, SetStoresAndBumpsRevision) {
  nsclient::core::tag_repository repo;
  EXPECT_EQ(repo.set("drives", "c:,d:"), set_result::changed);
  EXPECT_EQ(repo.get_revision(), 1u);
  const auto tags = repo.get_all();
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags.at("drives"), "c:,d:");
}

TEST(TagRepository, ResettingSameValueIsANoOp) {
  nsclient::core::tag_repository repo;
  repo.set("sqlserver", "detected");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(repo.set("sqlserver", "detected"), set_result::unchanged);
  EXPECT_EQ(repo.get_revision(), revision) << "revision must only move on real changes";
}

TEST(TagRepository, ChangingAValueBumpsRevision) {
  nsclient::core::tag_repository repo;
  repo.set("drives", "c:");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(repo.set("drives", "c:,d:"), set_result::changed);
  EXPECT_EQ(repo.get_revision(), revision + 1);
  EXPECT_EQ(repo.get_all().at("drives"), "c:,d:");
}

TEST(TagRepository, EmptyValueRemovesTheTag) {
  nsclient::core::tag_repository repo;
  repo.set("drives", "c:");
  EXPECT_EQ(repo.set("drives", ""), set_result::changed);
  EXPECT_TRUE(repo.get_all().empty());
}

TEST(TagRepository, RemovingAMissingTagIsANoOp) {
  nsclient::core::tag_repository repo;
  EXPECT_EQ(repo.set("missing", ""), set_result::unchanged);
  EXPECT_EQ(repo.get_revision(), 0u);
}

TEST(TagRepository, EmptyKeyIsRejected) {
  nsclient::core::tag_repository repo;
  EXPECT_EQ(repo.set("", "value"), set_result::rejected);
  EXPECT_TRUE(repo.get_all().empty());
}

TEST(TagRepository, KeyAtTheLimitIsAcceptedButOverIsRejected) {
  nsclient::core::tag_repository repo;
  const std::string at_limit(nsclient::core::tag_repository::max_key_length, 'k');
  const std::string over_limit(nsclient::core::tag_repository::max_key_length + 1, 'k');
  EXPECT_EQ(repo.set(at_limit, "v"), set_result::changed);
  EXPECT_EQ(repo.set(over_limit, "v"), set_result::rejected);
  EXPECT_EQ(repo.get_all().size(), 1u);
}

TEST(TagRepository, ValueAtTheLimitIsAcceptedButOverIsRejected) {
  nsclient::core::tag_repository repo;
  EXPECT_EQ(repo.set("k", std::string(nsclient::core::tag_repository::max_value_length, 'v')), set_result::changed);
  EXPECT_EQ(repo.set("k2", std::string(nsclient::core::tag_repository::max_value_length + 1, 'v')), set_result::rejected);
  EXPECT_EQ(repo.get_all().size(), 1u);
}

TEST(TagRepository, AnOversizedValueDoesNotClobberAnExistingTag) {
  nsclient::core::tag_repository repo;
  repo.set("drives", "c:");
  EXPECT_EQ(repo.set("drives", std::string(nsclient::core::tag_repository::max_value_length + 1, 'x')), set_result::rejected);
  EXPECT_EQ(repo.get_all().at("drives"), "c:") << "a value that cannot be stored must not erase the old one";
}

TEST(TagRepository, NewKeysAreRejectedAtCapacityButUpdatesAndRemovalsStillWork) {
  // Copy the cap into a local: passing the static member straight to EXPECT_EQ
  // binds it to a const& (an ODR-use), which would need an out-of-line
  // definition this header-only struct does not provide - MSVC tolerates it,
  // GCC/Clang reject it at link time.
  const std::size_t cap = nsclient::core::tag_repository::max_tags;
  nsclient::core::tag_repository repo;
  for (std::size_t i = 0; i < cap; ++i) {
    ASSERT_EQ(repo.set("tag" + std::to_string(i), "v"), set_result::changed);
  }
  EXPECT_EQ(repo.get_all().size(), cap);
  // A brand-new key past the cap is refused...
  EXPECT_EQ(repo.set("one-too-many", "v"), set_result::rejected);
  // ...but updating an existing key and shedding a tag must still work.
  EXPECT_EQ(repo.set("tag0", "changed"), set_result::changed);
  EXPECT_EQ(repo.set("tag1", ""), set_result::changed);
  EXPECT_EQ(repo.get_all().size(), cap - 1);
}
