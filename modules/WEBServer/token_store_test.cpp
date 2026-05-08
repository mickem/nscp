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

// Generating beyond the cap must not let the map grow unbounded. Earlier
// versions only pruned on lookup, so a workload of repeated logins with
// abandoned sessions could OOM the agent over time.
TEST(TokenStoreTest, GenerationBeyondCapEvictsOldest) {
  token_store store;
  // Generate slightly more than the documented cap. The store sweeps inside
  // generate_for(); after the loop the live token count must not exceed the
  // cap. We don't depend on the exact cap value - just that it's bounded.
  std::vector<std::string> tokens;
  for (int i = 0; i < 5000; ++i) {
    tokens.push_back(store.generate_for("user" + std::to_string(i)));
  }
  // Conservative upper bound: the implementation's kMaxTokens is 4096; we
  // assert "well under what we generated" rather than the exact value so
  // future tuning of the cap does not require a test edit.
  std::size_t live = 0;
  for (const auto &t : tokens) {
    if (store.is_valid(t)) ++live;
  }
  EXPECT_LT(live, 5000u) << "every token survived - eviction did not run";
  EXPECT_LE(live, 4096u) << "live count exceeded the documented cap";
}
