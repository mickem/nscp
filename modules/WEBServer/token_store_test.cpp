// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "token_store.hpp"

#include <gtest/gtest.h>

#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

TEST(TokenStoreTest, GenerateToken) {
  const std::string token1 = token_store::generate_token(32);
  EXPECT_EQ(token1.length(), 32);
  const std::string token2 = token_store::generate_token(32);
  EXPECT_EQ(token2.length(), 32);
  EXPECT_NE(token1, token2);
}

TEST(TokenStoreTest, GenerateTokenCharsetAndLengths) {
  // Every character must come from the [0-9A-Za-z] alphabet regardless of the
  // requested length (guards against the CSPRNG rejection-sampling loop
  // emitting a stray byte).
  const auto is_alnum = [](char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
  for (const int len : {1, 2, 16, 32, 64, 129}) {
    const std::string t = token_store::generate_token(len);
    EXPECT_EQ(static_cast<int>(t.size()), len);
    for (const char c : t) EXPECT_TRUE(is_alnum(c)) << "unexpected char in token of len " << len;
  }
  EXPECT_TRUE(token_store::generate_token(0).empty());
  EXPECT_TRUE(token_store::generate_token(-5).empty());
}

TEST(TokenStoreTest, GenerateTokenNoDuplicatesInLargeSample) {
  // A predictable/degenerate RNG would collide quickly; a CSPRNG will not.
  std::set<std::string> seen;
  for (int i = 0; i < 2000; ++i) {
    const std::string t = token_store::generate_token(32);
    EXPECT_EQ(t.size(), 32u);
    EXPECT_TRUE(seen.insert(t).second) << "duplicate token generated";
  }
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

TEST(TokenStoreTest, GetUserDoesNotResolveExpiredToken) {
  // get_user() must honour expiry on its own, not only via a preceding
  // is_valid(): an expired token must never name a user.
  token_store store;
  const std::string user = "test_user";
  const std::string token = store.generate_for(user);
  EXPECT_EQ(store.get_user(token, token_store::now()), user);
  EXPECT_EQ(store.get_user(token, token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS - 1)), user);
  EXPECT_EQ(store.get_user(token, token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS + 1)), "");
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

// --- CSPRNG failure handling -------------------------------------------------
//
// generate_token() draws from RAND_bytes. These tests swap in a deterministic
// stand-in via the test seam so both the fail-closed path and the
// rejection-sampling loop can be exercised without depending on the real RNG.

namespace {
// Restores the real CSPRNG however the test exits.
struct scoped_rand_override {
  explicit scoped_rand_override(token_store::rand_bytes_fn fn) { token_store::set_rand_bytes_for_test(fn); }
  ~scoped_rand_override() { token_store::set_rand_bytes_for_test(nullptr); }
};

int failing_rand_bytes(unsigned char *, int) { return 0; }

// Emits 0,1,2,... so every byte below the reject limit maps to a known char.
unsigned char g_counter = 0;
int counting_rand_bytes(unsigned char *buf, const int num) {
  for (int i = 0; i < num; ++i) buf[i] = g_counter++;
  return 1;
}

// First fill is entirely 0xFF (255) - at or above the reject limit (248), so
// every byte must be discarded; the second fill is usable. Proves the loop
// re-draws rather than folding an out-of-range byte into the alphabet.
int g_call_count = 0;
int reject_then_accept_rand_bytes(unsigned char *buf, const int num) {
  ++g_call_count;
  for (int i = 0; i < num; ++i) buf[i] = (g_call_count == 1) ? 0xFF : static_cast<unsigned char>(i);
  return 1;
}
}  // namespace

TEST(TokenStoreTest, GenerateTokenFailsClosedWhenCsprngFails) {
  // A CSPRNG that exists but fails must NOT silently fall back to a weaker
  // source - it must yield nothing so the caller refuses to issue a credential.
  const scoped_rand_override guard(&failing_rand_bytes);
  EXPECT_TRUE(token_store::generate_token(32).empty());
  EXPECT_TRUE(token_store::generate_token(1).empty());
}

TEST(TokenStoreTest, GenerateForIssuesNoSessionWhenCsprngFails) {
  token_store store;
  const scoped_rand_override guard(&failing_rand_bytes);
  const std::string token = store.generate_for("test_user");
  EXPECT_TRUE(token.empty()) << "a session was created from a failed CSPRNG";
  // Critically, no entry may have been stored under the empty key - that would
  // hand a valid session to every request that presents no token at all.
  EXPECT_FALSE(store.is_valid(""));
  EXPECT_EQ(store.get_user(""), "");
  std::string user;
  EXPECT_FALSE(store.validate("", user));
}

TEST(TokenStoreTest, GenerateTokenRedrawsAfterRejectedBytes) {
  // A byte >= the reject limit must be discarded and re-drawn, not folded into
  // the alphabet with modulo bias (0xFF % 62 would otherwise yield 'v').
  static constexpr char kAlphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  g_call_count = 0;
  const scoped_rand_override guard(&reject_then_accept_rand_bytes);
  const std::string token = token_store::generate_token(5);
  ASSERT_EQ(token.size(), 5u);
  EXPECT_EQ(g_call_count, 2) << "the all-rejected fill did not force a re-draw";
  // Second fill is 0,1,2,3,4 -> the first five alphabet characters.
  for (std::size_t i = 0; i < token.size(); ++i) {
    EXPECT_EQ(token[i], kAlphanum[i]) << "at index " << i;
  }
}

TEST(TokenStoreTest, GenerateTokenUsesAllBytesBelowRejectLimit) {
  // With a counting RNG, byte i maps to alphanum[i % 62] for i < 248 and is
  // skipped for i >= 248. Verify the mapping directly for the first bytes.
  static constexpr char kAlphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  g_counter = 0;
  const scoped_rand_override guard(&counting_rand_bytes);
  const std::string token = token_store::generate_token(10);
  ASSERT_EQ(token.size(), 10u);
  for (std::size_t i = 0; i < token.size(); ++i) {
    EXPECT_EQ(token[i], kAlphanum[i % 62]) << "at index " << i;
  }
}

// --- validate(): validity and identity from one observation ------------------

TEST(TokenStoreTest, ValidateResolvesUserAndHonoursExpiry) {
  token_store store;
  const std::string user = "test_user";
  const std::string token = store.generate_for(user);

  std::string got;
  EXPECT_TRUE(store.validate(token, got, token_store::now()));
  EXPECT_EQ(got, user);

  got.clear();
  EXPECT_TRUE(store.validate(token, got, token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS - 1)));
  EXPECT_EQ(got, user);

  // Expired: must report failure AND leave the out-param untouched, so a
  // caller that forgets to check the return value cannot pick up a username.
  got = "sentinel";
  EXPECT_FALSE(store.validate(token, got, token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS + 1)));
  EXPECT_EQ(got, "sentinel");
}

TEST(TokenStoreTest, ValidateEvictsExpiredToken) {
  token_store store;
  const std::string token = store.generate_for("test_user");
  std::string user;
  EXPECT_FALSE(store.validate(token, user, token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS + 1)));
  // The entry is gone, so even a later call with a valid clock cannot resolve
  // it - this is what makes validate() a drop-in for is_valid()'s eviction.
  EXPECT_FALSE(store.validate(token, user, token_store::now()));
  EXPECT_EQ(store.get_user(token, token_store::now()), "");
}

TEST(TokenStoreTest, ValidateRejectsUnknownToken) {
  token_store store;
  std::string user = "sentinel";
  EXPECT_FALSE(store.validate("no-such-token", user));
  EXPECT_EQ(user, "sentinel");
}

// --- Hash-keyed storage -------------------------------------------------------
//
// The map is keyed by the SHA-256 of the token, never by the token itself, so
// a memory dump does not hand anybody a working bearer credential. These
// tests pin that.

TEST(TokenStoreTest, MapIsKeyedByTheHashNotTheRawToken) {
  token_store store;
  const std::string token = store.generate_for("test_user");
  ASSERT_FALSE(token.empty());
  const std::string key = token_store::key_for(token);
  if (token_store::has_hashing()) {
    EXPECT_NE(key, token) << "key_for returned the raw token";
    EXPECT_EQ(key.size(), 64u) << "a SHA-256 hex digest is 64 characters";
  }
  // Whatever the key is, looking the session up by the raw token works and
  // looking it up by the key does not (the key is not itself a token).
  EXPECT_TRUE(store.is_valid(token));
  EXPECT_EQ(store.get_user(token), "test_user");
  if (token_store::has_hashing()) {
    EXPECT_FALSE(store.is_valid(key));
  }
}

TEST(TokenStoreTest, HashTokenIsStableAndDistinct) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  EXPECT_EQ(token_store::hash_token("abc"), token_store::hash_token("abc"));
  EXPECT_NE(token_store::hash_token("abc"), token_store::hash_token("abd"));
  // Known answer: SHA-256("abc").
  EXPECT_EQ(token_store::hash_token("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// --- Fail-closed on a digest failure -----------------------------------------
//
// In a hashing build the map key is the hash. If the digest ever fails, the
// only wrong things to do are to store the raw token (a key no later lookup
// would hash to - the client holds a credential that 403s until it expires
// and that revoke() cannot find) or to store under "", which is not a key any
// token maps to: while the digest stays down every lookup in the store
// resolves to that one entry, and once it recovers nothing reaches it again.
// The right thing is what a failed RNG already does: refuse to mint.

namespace {
std::string failing_digest(const std::string &) { return std::string(); }
std::string constant_digest(const std::string &) { return std::string(64, 'c'); }

struct scoped_digest_override {
  explicit scoped_digest_override(token_store::digest_fn fn) { token_store::set_digest_for_test(fn); }
  ~scoped_digest_override() { token_store::set_digest_for_test(nullptr); }
};
}  // namespace

TEST(TokenStoreTest, GenerateForRefusesToMintWhenTheDigestFails) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  {
    scoped_digest_override guard(failing_digest);
    EXPECT_TRUE(store.generate_for("test_user").empty()) << "a session was minted without a usable key";
  }
  // Recovery: once the digest works again, sessions are minted as normal.
  const std::string token = store.generate_for("test_user");
  EXPECT_FALSE(token.empty());
  EXPECT_TRUE(store.is_valid(token));
}

TEST(TokenStoreTest, DigestFailureDoesNotOpenTheStore) {
  // A live session exists; the digest then fails. Neither an empty token nor
  // a random one may match anything - "" is not a key in the map.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string token = store.generate_for("test_user");
  ASSERT_FALSE(token.empty());
  scoped_digest_override guard(failing_digest);
  EXPECT_FALSE(store.is_valid(""));
  EXPECT_FALSE(store.is_valid(token)) << "lookups fail closed while the digest is down";
  EXPECT_EQ(store.get_user(token), "");
}

TEST(TokenStoreTest, TheMapIsKeyedByWhatTheDigestReturns) {
  // Pin that every path goes through the same digest: with a constant digest
  // two different tokens collide (the second mint overwrites the first), and
  // any string at all resolves to that one session.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  scoped_digest_override guard(constant_digest);
  const std::string first = store.generate_for("first");
  const std::string second = store.generate_for("second");
  ASSERT_NE(first, second);
  EXPECT_EQ(store.get_user(first), "second");
  EXPECT_EQ(store.get_user("anything"), "second");
}

TEST(TokenStoreTest, TheDigestSeamEngagesInEveryBuild) {
  // No has_hashing() guard, and that is the point: an installed seam IS this
  // build's hash function, so the store keys by what the seam returns whether
  // or not OpenSSL is present. Before has_hashing() accounted for the
  // override, key_for() skipped hash_token() in a no-OpenSSL build and
  // set_digest_for_test was a silent no-op there.
  token_store store;
  const scoped_digest_override guard(constant_digest);
  ASSERT_TRUE(token_store::has_hashing()) << "an installed digest seam is a hash function";
  const std::string token = store.generate_for("seam_user");
  ASSERT_FALSE(token.empty());
  EXPECT_EQ(token_store::key_for(token), constant_digest(token)) << "the key is not what the seam returned";
  EXPECT_NE(token_store::key_for(token), token) << "the map is keyed by the raw token";
  // The constant digest collapses every token onto one key, so any string at
  // all resolves to the single session - proof the lookups hash too.
  EXPECT_EQ(store.get_user("anything at all"), "seam_user");
}

TEST(TokenStoreTest, DigestFailureFailsClosedInEveryBuild) {
  // Same reasoning for the fail-closed path: with the seam installed before
  // the mint, generate_for refuses in any build.
  token_store store;
  const scoped_digest_override guard(failing_digest);
  ASSERT_TRUE(token_store::has_hashing());
  EXPECT_TRUE(store.generate_for("test_user").empty()) << "a session was minted without a usable key";
}

TEST(TokenStoreTest, RevokeByRawTokenRemovesTheHashedEntry) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string token = store.generate_for("test_user");
  ASSERT_TRUE(store.is_valid(token));
  // The client sends the raw token; the entry lives under its hash.
  store.revoke(token);
  EXPECT_FALSE(store.is_valid(token));
  // Revoking by the hash does nothing: the hash is not a credential.
  const std::string other = store.generate_for("test_user");
  store.revoke(token_store::hash_token(other));
  EXPECT_TRUE(store.is_valid(other));
}

TEST(TokenStoreTest, RevokeTokensForUserFindsHashedEntries) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string mine = store.generate_for("me");
  const std::string theirs = store.generate_for("someone_else");
  store.revoke_tokens_for_user("me");
  EXPECT_FALSE(store.is_valid(mine));
  EXPECT_TRUE(store.is_valid(theirs));
}
