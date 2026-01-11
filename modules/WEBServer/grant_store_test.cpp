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
