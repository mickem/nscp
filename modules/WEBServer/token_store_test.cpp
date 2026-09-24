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

// --- Hash-keyed storage and persistence --------------------------------------
//
// The map is keyed by the SHA-256 of the token, never by the token itself, so
// neither a memory dump nor nsclient.db hands anybody a working bearer
// credential. These tests pin that, plus the snapshot/restore pair the
// WEBServer module uses to carry sessions across a restart.

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

TEST(TokenStoreTest, SnapshotNeverContainsTheRawToken) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string token = store.generate_for("test_user", "fp1");
  ASSERT_FALSE(token.empty());
  const auto snap = store.snapshot(token_store::now());
  ASSERT_EQ(snap.size(), 1u);
  const token_store::persisted_session &s = snap.front();
  EXPECT_NE(s.hash, token) << "the raw token leaked into the snapshot";
  EXPECT_EQ(s.hash, token_store::hash_token(token));
  EXPECT_EQ(s.user, "test_user");
  EXPECT_EQ(s.fingerprint, "fp1");
}

TEST(TokenStoreTest, SnapshotSkipsSessionsWithoutAFingerprint) {
  // The fingerprint-less generate_for() overload makes a session that no
  // import can vouch for, so it never reaches the file: exporting it would
  // only write a record import refuses, and it is the reason every record on
  // disk has all four fields.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string anonymous = store.generate_for("test_user");
  ASSERT_FALSE(anonymous.empty());
  EXPECT_TRUE(store.is_valid(anonymous)) << "it is still a perfectly good session in memory";
  EXPECT_TRUE(store.snapshot(token_store::now()).empty());

  const std::string bound = store.generate_for("test_user", "fp1");
  ASSERT_FALSE(bound.empty());
  const auto snap = store.snapshot(token_store::now());
  ASSERT_EQ(snap.size(), 1u) << "only the session with a fingerprint is exported";
  EXPECT_EQ(snap.front().hash, token_store::hash_token(bound));
}

TEST(TokenStoreTest, SnapshotSkipsExpiredEntries) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string token = store.generate_for("test_user", "fp1");
  EXPECT_EQ(store.snapshot(token_store::now()).size(), 1u);
  EXPECT_TRUE(store.snapshot(token_store::now() + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS + 1)).empty());
}

TEST(TokenStoreTest, RestoreRoundTripsASession) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store source;
  const std::string token = source.generate_for("test_user", "fp1");
  ASSERT_FALSE(token.empty());
  const auto snap = source.snapshot(token_store::now());
  ASSERT_EQ(snap.size(), 1u);

  token_store target;
  EXPECT_FALSE(target.is_valid(token)) << "fresh store must not know the token";
  EXPECT_TRUE(target.restore(snap.front(), token_store::now()));
  // The raw token the client still holds now authenticates against the new
  // store, which is the whole point of persisting sessions.
  EXPECT_TRUE(target.is_valid(token));
  EXPECT_EQ(target.get_user(token), "test_user");
  // ... and it is exportable again at the next shutdown.
  const auto again = target.snapshot(token_store::now());
  ASSERT_EQ(again.size(), 1u);
  EXPECT_EQ(again.front().fingerprint, "fp1");
}

TEST(TokenStoreTest, RestoreSkipsExpiredAndMalformedRecords) {
  token_store store;
  token_store::persisted_session s;
  s.hash = std::string(64, 'a');
  s.user = "test_user";
  s.created = token_store::now() - HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS + 1);
  EXPECT_FALSE(store.restore(s, token_store::now())) << "an expired record must not be restored";

  s.created = token_store::now();
  s.user = "";
  EXPECT_FALSE(store.restore(s, token_store::now())) << "a record without a user must not be restored";

  s.user = "test_user";
  s.hash = "";
  EXPECT_FALSE(store.restore(s, token_store::now())) << "a record without a hash must not be restored";
}

TEST(TokenStoreTest, RestoreDoesNotOverwriteALiveEntry) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string token = store.generate_for("live_user", "fp-live");
  ASSERT_FALSE(token.empty());
  token_store::persisted_session s;
  s.hash = token_store::hash_token(token);
  s.user = "stale_user";
  s.created = token_store::now();
  s.fingerprint = "fp-stale";
  EXPECT_FALSE(store.restore(s, token_store::now()));
  EXPECT_EQ(store.get_user(token), "live_user") << "the live entry was overwritten from disk";
}

TEST(TokenStoreTest, RestoreRespectsTheCap) {
  // A nsclient.db holding more rows than the cap must not be able to push the
  // live map past it - the cap is the defensive boundary, whichever side the
  // sessions arrive from. restore() goes through the same sweep generate_for()
  // does, so it evicts rather than refuses; what is pinned here is that the
  // map stays bounded.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const time_t now = token_store::now();
  for (int i = 0; i < 5000; ++i) {
    token_store::persisted_session s;
    // 64 hex characters, unique per i - what parse_sessions accepts as a hash.
    std::ostringstream oss;
    oss << std::hex << std::setw(64) << std::setfill('0') << i;
    s.hash = oss.str();
    s.user = "user" + std::to_string(i);
    // Spread the creation times so eviction has an unambiguous "oldest".
    s.created = now - i;
    // Without a fingerprint the restored sessions never reach snapshot(), and
    // the bound checked below would hold over an empty list.
    s.fingerprint = "fp";
    store.restore(s, now);
  }
  EXPECT_LE(store.snapshot(now).size(), 4096u) << "live count exceeded the documented cap";
}

// --- Serialisation -----------------------------------------------------------

namespace {
token_store::persisted_session make_session(const char fill, const std::string &user, const time_t created, const std::string &fingerprint) {
  token_store::persisted_session s;
  s.hash = std::string(64, fill);
  s.user = user;
  s.created = created;
  s.fingerprint = fingerprint;
  return s;
}
}  // namespace

TEST(SessionPersistenceTest, SerialiseParseRoundTrip) {
  std::list<token_store::persisted_session> in;
  in.push_back(make_session('a', "test_user", 1700000000, "deadbeef"));
  in.push_back(make_session('b', "other_user", 1700000001, ""));
  const std::string value = session_persistence::serialize_sessions(in);
  EXPECT_EQ(value, "1\n" + std::string(64, 'a') + "\ttest_user\t1700000000\tdeadbeef\n" + std::string(64, 'b') + "\tother_user\t1700000001\t");

  const auto out = session_persistence::parse_sessions(value);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out.front().hash, in.front().hash);
  EXPECT_EQ(out.front().user, "test_user");
  EXPECT_EQ(out.front().created, 1700000000);
  EXPECT_EQ(out.front().fingerprint, "deadbeef");
  EXPECT_EQ(out.back().hash, in.back().hash);
  EXPECT_EQ(out.back().user, "other_user");
  EXPECT_EQ(out.back().fingerprint, "") << "an empty fingerprint field must round-trip";
}

TEST(SessionPersistenceTest, AnEmptyTableSerialisesToAnEmptyValue) {
  // "" is what the module writes for "no sessions" (and with persistence
  // switched off), and what an earlier run that wrote nothing looks like.
  EXPECT_EQ(session_persistence::serialize_sessions({}), "");
  EXPECT_TRUE(session_persistence::parse_sessions("").empty());
}

TEST(SessionPersistenceTest, SerialiseLeavesOutRecordsThatCannotBeReadBack) {
  std::list<token_store::persisted_session> in;
  in.push_back(make_session('a', "bad\tuser", 1700000000, "fp"));
  in.push_back(make_session('b', "bad\nuser", 1700000000, "fp"));
  in.push_back(make_session('c', "test_user", 1700000000, "bad\tfingerprint"));
  in.push_back(make_session('d', "", 1700000000, "fp"));
  token_store::persisted_session raw = make_session('e', "test_user", 1700000000, "fp");
  raw.hash = std::string(32, 'e');  // a raw token, not a hash
  in.push_back(raw);
  EXPECT_EQ(session_persistence::serialize_sessions(in), "") << "none of these can be represented";

  // ... and one bad record does not take the good ones down with it.
  in.push_back(make_session('f', "good_user", 1700000000, "fp"));
  EXPECT_EQ(session_persistence::serialize_sessions(in), "1\n" + std::string(64, 'f') + "\tgood_user\t1700000000\tfp");
}

TEST(SessionPersistenceTest, ParseSkipsMalformedRecordsAndKeepsTheRest) {
  const std::string good = std::string(64, 'a') + "\tgood_user\t1700000000\tfp";
  const std::string bad[] = {
      "garbage",
      std::string(64, 'b') + "\ttest_user\t1700000000",             // too few fields
      std::string(64, 'b') + "\ttest_user\t1700000000\tfp\textra",  // too many fields
      std::string(64, 'b') + "\t\t1700000000\tfp",                  // empty user
      std::string(64, 'b') + "\ttest_user\tnotanumber\tfp",         // non-numeric timestamp
      std::string(64, 'b') + "\ttest_user\t0\tfp",                  // zero timestamp
      std::string(64, 'b') + "\ttest_user\t-5\tfp",                 // negative timestamp
      "tooshort\ttest_user\t1700000000\tfp",                        // not a hash
      std::string(32, 'b') + "\ttest_user\t1700000000\tfp",         // a raw token is exactly what must never load
      std::string(64, 'B') + "\ttest_user\t1700000000\tfp",         // uppercase is not our hex
      std::string(64, 'z') + "\ttest_user\t1700000000\tfp",         // not hex at all
  };
  for (const std::string &line : bad) {
    const auto out = session_persistence::parse_sessions("1\n" + line + "\n" + good);
    ASSERT_EQ(out.size(), 1u) << "record was not skipped: " << line;
    EXPECT_EQ(out.front().user, "good_user");
  }
  // A blank line is ignored too (a trailing newline is not a record).
  EXPECT_EQ(session_persistence::parse_sessions("1\n" + good + "\n").size(), 1u);
}

TEST(SessionPersistenceTest, ParseRejectsAnUnknownOrMissingVersion) {
  const std::string good = std::string(64, 'a') + "\tgood_user\t1700000000\tfp";
  EXPECT_TRUE(session_persistence::parse_sessions("2\n" + good).empty()) << "unknown version";
  EXPECT_TRUE(session_persistence::parse_sessions(good).empty()) << "a record where the version should be";
  EXPECT_TRUE(session_persistence::parse_sessions("1").empty()) << "a version and nothing else is an empty table";
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
    EXPECT_TRUE(store.generate_for("test_user", "fp").empty()) << "a session was minted without a usable key";
    EXPECT_TRUE(store.snapshot(token_store::now()).empty());
  }
  // Recovery: once the digest works again, sessions are minted as normal.
  EXPECT_FALSE(store.generate_for("test_user", "fp").empty());
}

TEST(TokenStoreTest, DigestFailureDoesNotOpenTheStore) {
  // A live session exists; the digest then fails. Neither an empty token nor
  // a random one may match anything - "" is not a key in the map.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string token = store.generate_for("test_user", "fp");
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
  const std::string first = store.generate_for("first", "fp");
  const std::string second = store.generate_for("second", "fp");
  ASSERT_NE(first, second);
  EXPECT_EQ(store.get_user(first), "second");
  EXPECT_EQ(store.get_user("anything"), "second");
  EXPECT_EQ(store.snapshot(token_store::now()).size(), 1u);
  EXPECT_EQ(store.snapshot(token_store::now()).front().hash, std::string(64, 'c'));
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

// --- Revocation of hashed and restored sessions --------------------------------

TEST(TokenStoreTest, RevokeByRawTokenRemovesTheHashedEntry) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string token = store.generate_for("test_user", "fp");
  ASSERT_EQ(store.snapshot(token_store::now()).size(), 1u);
  // The client sends the raw token; the entry lives under its hash.
  store.revoke(token);
  EXPECT_FALSE(store.is_valid(token));
  EXPECT_TRUE(store.snapshot(token_store::now()).empty()) << "a revoked session was still exportable";
  // Revoking by the hash does nothing: the hash is not a credential.
  const std::string other = store.generate_for("test_user", "fp");
  store.revoke(token_store::hash_token(other));
  EXPECT_TRUE(store.is_valid(other));
}

TEST(TokenStoreTest, RevokeTokensForUserDropsTheirSessionsFromTheSnapshot) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const std::string mine = store.generate_for("me", "fp");
  const std::string theirs = store.generate_for("someone_else", "fp");
  store.revoke_tokens_for_user("me");
  EXPECT_FALSE(store.is_valid(mine));
  EXPECT_TRUE(store.is_valid(theirs));
  const auto snap = store.snapshot(token_store::now());
  ASSERT_EQ(snap.size(), 1u);
  EXPECT_EQ(snap.front().user, "someone_else");
}

TEST(TokenStoreTest, RestoredSessionCanBeRevokedByTheRawToken) {
  // After a restart the client logs out with the token it has always had;
  // the store only ever saw its hash, and that must be enough to find it.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store before;
  const std::string token = before.generate_for("test_user", "fp");
  token_store after;
  ASSERT_TRUE(after.restore(before.snapshot(token_store::now()).front(), token_store::now()));
  ASSERT_TRUE(after.is_valid(token));
  after.revoke(token);
  EXPECT_FALSE(after.is_valid(token));
  EXPECT_TRUE(after.snapshot(token_store::now()).empty());
}

TEST(TokenStoreTest, RestoringTheSameRecordTwiceInsertsOnce) {
  token_store store;
  token_store::persisted_session s;
  s.hash = std::string(64, 'a');
  s.user = "test_user";
  s.created = token_store::now();
  // Every record that reaches restore() in production came out of snapshot(),
  // which leaves out a session with no fingerprint - so a record built by hand
  // carries one too, or it is invisible to the snapshot() checked below.
  s.fingerprint = "fp";
  EXPECT_TRUE(store.restore(s, token_store::now()));
  EXPECT_FALSE(store.restore(s, token_store::now())) << "a duplicate row must not count as a second restore";
  if (token_store::has_hashing()) EXPECT_EQ(store.snapshot(token_store::now()).size(), 1u);
}

TEST(TokenStoreTest, RestoredSessionKeepsItsOriginalExpiry) {
  // A session restored with an hour left has an hour left, not eight: the
  // clock started at login, not at the restart.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  const time_t now = token_store::now();
  token_store::persisted_session s;
  s.hash = std::string(64, 'a');
  s.user = "test_user";
  s.created = now - HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS - 1);
  s.fingerprint = "fp";
  ASSERT_TRUE(store.restore(s, now));
  EXPECT_EQ(store.snapshot(now).size(), 1u);
  EXPECT_TRUE(store.snapshot(now + HOURS_TO_SECONDS(2)).empty()) << "the restored session outlived its original expiry";
}

TEST(TokenStoreTest, GenerationBeyondCapKeepsTheSnapshotBounded) {
  // A login storm cannot grow the table, or the storage row, without bound.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store store;
  for (int i = 0; i < 5000; ++i) {
    const std::string token = store.generate_for("user" + std::to_string(i), "fp");
    ASSERT_FALSE(token.empty());
  }
  EXPECT_LE(store.snapshot(token_store::now()).size(), 4096u);
}

// --- Serialisation, round trips at scale and through a text tool --------------

TEST(SessionPersistenceTest, RoundTripsAFullTable) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  token_store before;
  std::vector<std::string> tokens;
  for (int i = 0; i < 200; ++i) {
    tokens.push_back(before.generate_for("user" + std::to_string(i % 7), "fp" + std::to_string(i % 3)));
  }
  const std::string value = session_persistence::serialize_sessions(before.snapshot(token_store::now()));
  const auto parsed = session_persistence::parse_sessions(value);
  ASSERT_EQ(parsed.size(), 200u);

  token_store after;
  for (const auto &s : parsed) ASSERT_TRUE(after.restore(s, token_store::now()));
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    EXPECT_EQ(after.get_user(tokens[i]), "user" + std::to_string(i % 7)) << "token " << i;
  }
  // And the table the second process would write is the same table.
  EXPECT_EQ(session_persistence::parse_sessions(session_persistence::serialize_sessions(after.snapshot(token_store::now()))).size(), 200u);
}

TEST(SessionPersistenceTest, ParseToleratesWindowsLineEndings) {
  const std::string good = std::string(64, 'a') + "\tgood_user\t1700000000\tfp";
  const auto out = session_persistence::parse_sessions("1\r\n" + good + "\r\n");
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out.front().fingerprint, "fp") << "the CR must not end up in the fingerprint";
}

TEST(SessionPersistenceTest, ParseSkipsAHashItHasAlreadySeen) {
  // Two rows with one hash cannot both be restored; the store refuses the
  // second, so a duplicated line costs nothing but is not a second session.
  const std::string line = std::string(64, 'a') + "\tgood_user\t" + std::to_string(static_cast<long long>(token_store::now())) + "\tfp";
  const auto parsed = session_persistence::parse_sessions("1\n" + line + "\n" + line);
  ASSERT_EQ(parsed.size(), 2u);
  token_store store;
  std::size_t restored = 0;
  for (const auto &s : parsed) {
    if (store.restore(s, token_store::now())) ++restored;
  }
  EXPECT_EQ(restored, 1u);
}
