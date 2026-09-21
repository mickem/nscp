// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "grant_store.hpp"

#include <gtest/gtest.h>

TEST(GrantStoreTest, BasicGrant) {
  grant_store store;
  store.add_user("user1", "role1");
  store.add_role("role1", "permission1");

  EXPECT_TRUE(store.validate("user1", "permission1"));
  EXPECT_FALSE(store.validate("user1", "permission2"));
}

TEST(GrantStoreTest, MultipleGrants) {
  grant_store store;
  store.add_user("user1", "role1");
  store.add_role("role1", "perm1,perm2");

  EXPECT_TRUE(store.validate("user1", "perm1"));
  EXPECT_TRUE(store.validate("user1", "perm2"));
  EXPECT_FALSE(store.validate("user1", "perm3"));
}

TEST(GrantStoreTest, CumulativeGrants) {
  grant_store store;
  store.add_user("user1", "role1");
  store.add_role("role1", "perm1");
  store.add_role("role1", "perm2");

  EXPECT_TRUE(store.validate("user1", "perm1"));
  EXPECT_TRUE(store.validate("user1", "perm2"));
}

TEST(GrantStoreTest, WildcardGrant) {
  grant_store store;
  store.add_user("admin", "admin_role");
  store.add_role("admin_role", "*");

  EXPECT_TRUE(store.validate("admin", "anything"));
  EXPECT_TRUE(store.validate("admin", "foo.bar"));
}

TEST(GrantStoreTest, PartialWildcard) {
  grant_store store;
  store.add_user("user", "role");
  store.add_role("role", "foo.*");

  EXPECT_TRUE(store.validate("user", "foo.bar"));
  EXPECT_TRUE(store.validate("user", "foo.baz"));
  EXPECT_FALSE(store.validate("user", "bar.foo"));
}

TEST(GrantStoreTest, Hierarchy) {
  grant_store store;
  store.add_user("user", "role");
  store.add_role("role", "a.b");

  EXPECT_TRUE(store.validate("user", "a.b"));
  EXPECT_FALSE(store.validate("user", "a"));
  EXPECT_FALSE(store.validate("user", "a.b.c"));
  EXPECT_FALSE(store.validate("user", "a.c"));
  EXPECT_FALSE(store.validate("user", "x.b"));
}

TEST(GrantStoreTest, RemoveUser) {
  grant_store store;
  store.add_user("user1", "role1");
  store.add_role("role1", "perm1");

  EXPECT_TRUE(store.validate("user1", "perm1"));
  store.remove_user("user1");
  EXPECT_FALSE(store.validate("user1", "perm1"));
}

TEST(GrantStoreTest, RemoveRole) {
  grant_store store;
  store.add_user("user1", "role1");
  store.add_role("role1", "perm1");

  EXPECT_TRUE(store.validate("user1", "perm1"));
  store.remove_role("role1");
  EXPECT_FALSE(store.validate("user1", "perm1"));
}

TEST(GrantStoreTest, Clear) {
  grant_store store;
  store.add_user("user1", "role1");
  store.add_role("role1", "perm1");

  store.clear();
  EXPECT_FALSE(store.validate("user1", "perm1"));
}

TEST(GrantStoreTest, UnknownUser) {
  grant_store store;
  EXPECT_FALSE(store.validate("nonexistent", "perm"));
}

TEST(GrantStoreTest, UserWithoutRole) {
  grant_store store;
  store.add_user("user1", "");
  EXPECT_FALSE(store.validate("user1", "perm"));
}

TEST(GrantStoreTest, GetRoleReturnsTheMappingOrNothing) {
  grant_store store;
  store.add_role("role1", "permission1");
  EXPECT_EQ(store.get_role("nobody"), "");
  store.add_user("user1", "role1");
  EXPECT_EQ(store.get_role("user1"), "role1");
  store.add_user("user1", "role2");
  EXPECT_EQ(store.get_role("user1"), "role2") << "re-adding a user replaces the role";
  store.remove_user("user1");
  EXPECT_EQ(store.get_role("user1"), "");
}

TEST(GrantStoreTest, ValidateDoesNotMutateTheStore) {
  // A permission check is a lookup, not a write. `users[uid]` used to insert
  // an empty role for every uid it was asked about, so each check for an
  // unknown user grew the map for the lifetime of the process; asking through
  // a const reference is what keeps that from coming back.
  grant_store store;
  store.add_role("role1", "permission1");
  store.add_user("user1", "role1");
  const grant_store &read_only = store;
  EXPECT_TRUE(read_only.validate("user1", "permission1"));
  EXPECT_FALSE(read_only.validate("ghost", "permission1"));
  EXPECT_EQ(read_only.get_role("ghost"), "");
}
