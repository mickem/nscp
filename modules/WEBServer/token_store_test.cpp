#include "token_store.hpp"

#include <gtest/gtest.h>

TEST(TokenStoreTest, GenerateToken) {
  const std::string token1 = token_store::generate_token(32);
  EXPECT_EQ(token1.length(), 32);
  const std::string token2 = token_store::generate_token(32);
  EXPECT_EQ(token2.length(), 32);
  EXPECT_NE(token1, token2);
}

TEST(TokenStoreTest, GenerateForUser) {
  token_store store;
  const std::string user = "test_user";
  const std::string token = store.generate_for(user);
  EXPECT_EQ(token.length(), 32);
  EXPECT_EQ(store.get_user(token), user);
}

TEST(TokenStoreTest, IsValid) {
  token_store store;
  const std::string user = "test_user";
  const std::string token = store.generate_for(user);
  EXPECT_TRUE(store.is_valid(token, token_store::now()));
  EXPECT_FALSE(store.is_valid("invalid_token", token_store::now()));
}

TEST(TokenStoreTest, RevokeToken) {
  token_store store;
  const std::string user = "test_user";
  const std::string token = store.generate_for(user);
  EXPECT_TRUE(store.is_valid(token, token_store::now()));
  store.revoke(token);
  EXPECT_FALSE(store.is_valid(token, token_store::now()));
}

TEST(TokenStoreTest, GetUser) {
  token_store store;
  const std::string user = "test_user";
  const std::string token = store.generate_for(user);
  EXPECT_EQ(store.get_user(token), user);
  EXPECT_EQ(store.get_user("invalid_token"), "");
}

TEST(TokenStoreTest, Expiration) {
  token_store store;
  const std::string user = "test_user";
  const std::string token = store.generate_for(user);
  EXPECT_TRUE(store.is_valid(token, token_store::now()));
  EXPECT_TRUE(store.is_valid(token, token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS - 1)));
  EXPECT_FALSE(store.is_valid(token, token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS + 1)));
}

TEST(TokenStoreTest, Grants) {
  token_store store;
  const std::string user = "test_user";
  const std::string role = "admin";
  const std::string grant = "read,write";

  store.add_user(user, role);
  store.add_grant(role, grant);

  EXPECT_TRUE(store.can(user, "read"));
  EXPECT_TRUE(store.can(user, "write"));
  EXPECT_FALSE(store.can(user, "execute"));
}

TEST(TokenStoreTest, WildcardGrant) {
  token_store store;
  const std::string user = "admin_user";
  const std::string role = "super_admin";
  const std::string grant = "*";

  store.add_user(user, role);
  store.add_grant(role, grant);

  EXPECT_TRUE(store.can(user, "read"));
  EXPECT_TRUE(store.can(user, "write"));
  EXPECT_TRUE(store.can(user, "anything"));
}

TEST(TokenStoreTest, HierarchicalGrant) {
  token_store store;
  const std::string user = "user";
  const std::string role = "viewer";
  const std::string grant = "module.read";

  store.add_user(user, role);
  store.add_grant(role, grant);

  EXPECT_TRUE(store.can(user, "module.read"));
  EXPECT_FALSE(store.can(user, "module.write"));
  EXPECT_FALSE(store.can(user, "other.read"));
}
