// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include "tag_repository.hpp"

TEST(TagRepository, StartsEmptyAtRevisionZero) {
  const nsclient::core::tag_repository repo;
  EXPECT_TRUE(repo.get_all().empty());
  EXPECT_EQ(repo.get_revision(), 0u);
}

TEST(TagRepository, SetStoresAndBumpsRevision) {
  nsclient::core::tag_repository repo;
  EXPECT_TRUE(repo.set("drives", "c:,d:"));
  EXPECT_EQ(repo.get_revision(), 1u);
  const auto tags = repo.get_all();
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags.at("drives"), "c:,d:");
}

TEST(TagRepository, ResettingSameValueIsANoOp) {
  nsclient::core::tag_repository repo;
  repo.set("sqlserver", "detected");
  const unsigned long long revision = repo.get_revision();
  EXPECT_FALSE(repo.set("sqlserver", "detected"));
  EXPECT_EQ(repo.get_revision(), revision) << "revision must only move on real changes";
}

TEST(TagRepository, ChangingAValueBumpsRevision) {
  nsclient::core::tag_repository repo;
  repo.set("drives", "c:");
  const unsigned long long revision = repo.get_revision();
  EXPECT_TRUE(repo.set("drives", "c:,d:"));
  EXPECT_EQ(repo.get_revision(), revision + 1);
  EXPECT_EQ(repo.get_all().at("drives"), "c:,d:");
}

TEST(TagRepository, EmptyValueRemovesTheTag) {
  nsclient::core::tag_repository repo;
  repo.set("drives", "c:");
  EXPECT_TRUE(repo.set("drives", ""));
  EXPECT_TRUE(repo.get_all().empty());
}

TEST(TagRepository, RemovingAMissingTagIsANoOp) {
  nsclient::core::tag_repository repo;
  EXPECT_FALSE(repo.set("missing", ""));
  EXPECT_EQ(repo.get_revision(), 0u);
}

TEST(TagRepository, EmptyKeyIsRejected) {
  nsclient::core::tag_repository repo;
  EXPECT_FALSE(repo.set("", "value"));
  EXPECT_TRUE(repo.get_all().empty());
}
