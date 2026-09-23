// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "session_manager_interface.hpp"

#include <Helpers.h>
#include <StreamResponse.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <list>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>

#include "password_hash.hpp"

// Provide the nscapi singleton expected by NSC_LOG_ERROR / NSC_DEBUG_MSG
// macros that fire from session_manager_interface.cpp. Production code gets
// this from `NSC_WRAP_DLL()` in the auto-generated module.cpp; in the test
// binary there's no plugin wrapper, so we instantiate it ourselves the same
// way modern_filter_test.cpp and friends do. Without it the linker has
// nothing to relocate against for any TU that emits the log macros.
nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();

// Mocks for Mongoose::Request only
namespace MockMongoose {
class Request {
 public:
  std::string remoteIp;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> variables;
  Request(const std::string& ip = "127.0.0.1") : remoteIp(ip) {}
  std::string getRemoteIp() const { return remoteIp; }
  bool hasVariable(const std::string& key) const { return variables.count(key) > 0; }
  std::string readHeader(const std::string& key) const {
    auto it = headers.find(key);
    return it != headers.end() ? it->second : "";
  }
  std::map<std::string, std::string>& get_headers() { return headers; }
  std::string get(const std::string& key, const std::string& def = "") const {
    auto it = variables.find(key);
    return it != variables.end() ? it->second : def;
  }
};
}  // namespace MockMongoose

// Minimal stub for session_manager_interface dependencies
class DummyUserManager {
 public:
  bool validate_user(const std::string& user, const std::string& pass) const { return user == "user" && pass == "pass"; }
  void add_user(const std::string&, const std::string&) {}
  bool has_user(const std::string& user) const { return user == "user"; }
};
class DummyTokenStore {
 public:
  bool is_valid(const std::string& token) const { return token == "validtoken"; }
  std::string generate_for(const std::string& user) const { return user + "_token"; }
  std::string get_user(const std::string& token) const { return token == "user_token" ? "user" : ""; }
  void add_user(const std::string&, const std::string&) {}
  void add_grant(const std::string&, const std::string&) {}
  bool can(const std::string& user, const std::string&) const { return user == "user" || user == "anonymous"; }
  void revoke(const std::string&) {}
};
class DummyAllowedHosts {
 public:
  bool is_allowed(const boost::asio::ip::address&, std::list<std::string>&) const { return true; }
  void refresh(std::list<std::string>&) {}
  void set_source(const std::string&) {}
  bool cached = false;
};

// Test fixture
class SessionManagerTest : public ::testing::Test {
 protected:
  session_manager_interface smi;
  void SetUp() override {
    smi.add_user("user", "foo", "password");
    smi.add_grant("foo", "something:read");
    // The `anonymous` role is gated on a settings flag (default off). The
    // existing tests for anonymous behaviour assume access is granted, so
    // flip the flag on for the fixture before registering the grant.
    smi.set_allow_anonymous(true);
    smi.add_user("anonymous", "anonymous", "anonymous");
    smi.add_grant("anonymous", "nothing:read");
    // allowed_hosts is now fail-closed when empty (L1). Configure 127.0.0.1
    // explicitly and run boot() so the source string is parsed into entries
    // - tests that hit is_allowed / is_logged_in expect localhost to be
    // permitted.
    smi.set_allowed_hosts("127.0.0.1");
    smi.boot();
  }
};

TEST_F(SessionManagerTest, BootReturnsNoErrors) {
  const auto errors = smi.boot();
  EXPECT_TRUE(errors.empty());
}

TEST_F(SessionManagerTest, AddAndValidateUser) {
  EXPECT_TRUE(smi.validate_user("user", "password"));
  EXPECT_TRUE(smi.has_user("user"));
}

TEST_F(SessionManagerTest, TokenGenerationAndValidation) {
  const std::string token = smi.generate_token("user");
  EXPECT_FALSE(token.empty());
  EXPECT_TRUE(smi.validate_token(token));
}

TEST_F(SessionManagerTest, StoreUserInResponseRecordsTheIdentity) {
  // Request-scoped context, not cookies: emitted as Set-Cookie these were a
  // second copy of the bearer on every authenticated response, which no
  // request path ever reads back.
  Mongoose::StreamResponse resp;
  EXPECT_TRUE(smi.store_user_in_response("user", resp));
  EXPECT_EQ(resp.getContext("uid"), "user");
  EXPECT_FALSE(resp.getContext("token").empty());
  EXPECT_TRUE(resp.getCookie("token").empty()) << "the session credential must not be sent back as a cookie";
  EXPECT_TRUE(resp.getCookie("uid").empty());
}

TEST_F(SessionManagerTest, StoreUserInResponseCanSkipMintingAToken) {
  // The path every non-login route takes. can() authorises from the uid alone,
  // so re-authenticating callers (an Icinga check_nscp_api poll) no longer add
  // an entry to the 4096-slot store and evict live UI sessions.
  Mongoose::StreamResponse resp;
  EXPECT_TRUE(smi.store_user_in_response("user", resp, false));
  EXPECT_EQ(resp.getContext("uid"), "user");
  EXPECT_TRUE(resp.getContext("token").empty());
}

TEST_F(SessionManagerTest, StoreUserInResponseFailsClosedWhenCsprngFails) {
  // A CSPRNG failure must not produce a half-formed session: no token cookie,
  // no uid context, and a false return so the caller refuses the request.
  Mongoose::StreamResponse resp;
  token_store::set_rand_bytes_for_test([](unsigned char*, int) { return 0; });
  const bool stored = smi.store_user_in_response("user", resp);
  token_store::set_rand_bytes_for_test(nullptr);
  EXPECT_FALSE(stored);
  EXPECT_TRUE(resp.getContext("token").empty());
  EXPECT_TRUE(resp.getContext("uid").empty());
}

TEST_F(SessionManagerTest, StoreSessionInResponseSetsCookies) {
  // The token and the user are now passed together - resolved from one locked
  // observation in token_store::validate() - rather than the user being looked
  // up again from the token.
  Mongoose::StreamResponse resp;
  smi.store_session_in_response("validtoken", "user", resp);
  EXPECT_EQ(resp.getContext("token"), "validtoken");
  EXPECT_EQ(resp.getContext("uid"), "user");
  EXPECT_TRUE(resp.getCookie("token").empty()) << "the session credential must not be sent back as a cookie";
}

TEST_F(SessionManagerTest, CanCheckPermissions) {
  Mongoose::StreamResponse resp;
  smi.store_user_in_response("user", resp);
  EXPECT_TRUE(smi.can("something:read", resp));
}

TEST_F(SessionManagerTest, CanCheckPermissionsAnonymous) {
  Mongoose::StreamResponse resp;
  EXPECT_TRUE(smi.can("nothing:read", resp));
}

TEST_F(SessionManagerTest, CanAcceptsAnyOneOfSeveralGrants) {
  // The multi-grant form is an OR: the endpoint is reachable by either
  // privilege. query_controller uses it to let both `queries.execute` and
  // `queries.execute.noargs` through the same door.
  Mongoose::StreamResponse resp;
  smi.store_user_in_response("user", resp);
  EXPECT_TRUE(smi.can(session_manager_interface::grant_options{"something:write", "something:read"}, resp));
}

TEST_F(SessionManagerTest, CanReportsWhichGrantMatched) {
  // query_controller branches on which of `queries.execute` /
  // `queries.execute.noargs` let the request in; reporting it from the walk
  // that authorised the call saves a second (mutex-guarded) walk of the tree.
  Mongoose::StreamResponse resp;
  smi.store_user_in_response("user", resp);
  std::string matched;
  EXPECT_TRUE(smi.can(session_manager_interface::grant_options{"something:write", "something:read"}, resp, &matched));
  EXPECT_EQ(matched, "something:read");
}

TEST_F(SessionManagerTest, CanLeavesTheMatchedGrantUntouchedWhenRefusing) {
  Mongoose::StreamResponse resp;
  smi.store_user_in_response("user", resp);
  std::string matched = "untouched";
  EXPECT_FALSE(smi.can(session_manager_interface::grant_options{"something:write", "other:read"}, resp, &matched));
  EXPECT_EQ(matched, "untouched");
}

TEST_F(SessionManagerTest, CanRefusesWhenNoneOfTheGrantsMatch) {
  Mongoose::StreamResponse resp;
  smi.store_user_in_response("user", resp);
  EXPECT_FALSE(smi.can(session_manager_interface::grant_options{"something:write", "other:read"}, resp));
}

TEST_F(SessionManagerTest, HasGrantDoesNotTouchTheResponse) {
  // has_grant() is the non-mutating twin of can(): call sites that branch on
  // a privilege (may this caller pass arguments?) must not have a 403 written
  // into the response as a side effect of asking.
  Mongoose::StreamResponse resp;
  smi.store_user_in_response("user", resp);
  EXPECT_FALSE(smi.has_grant("something:write", resp));
  EXPECT_TRUE(smi.has_grant("something:read", resp));
  EXPECT_EQ(resp.getCode(), 200);
}

TEST(SessionManagerNoArgs, ExecuteAndNoargsGrantsAreDisjoint) {
  // The `restricted` role's `queries.execute.noargs` must never widen into
  // the full `queries.execute` privilege, and an existing `queries.execute`
  // role must not accidentally be treated as a no-arguments one. Neither
  // implies the other; only the `*` wildcard confers both.
  session_manager_interface smi;
  smi.add_user("restricted", "restricted", "password");
  smi.add_grant("restricted", "public,queries.execute.noargs,login.get");
  smi.add_user("monitoring", "monitoring", "password");
  smi.add_grant("monitoring", "public,queries.execute,login.get");
  smi.add_user("admin", "full", "password");
  smi.add_grant("full", "*");

  Mongoose::StreamResponse restricted;
  smi.store_user_in_response("restricted", restricted);
  EXPECT_TRUE(smi.has_grant("queries.execute.noargs", restricted));
  EXPECT_FALSE(smi.has_grant("queries.execute", restricted));

  Mongoose::StreamResponse monitoring;
  smi.store_user_in_response("monitoring", monitoring);
  EXPECT_TRUE(smi.has_grant("queries.execute", monitoring));
  EXPECT_FALSE(smi.has_grant("queries.execute.noargs", monitoring));

  Mongoose::StreamResponse admin;
  smi.store_user_in_response("admin", admin);
  EXPECT_TRUE(smi.has_grant("queries.execute", admin));
  EXPECT_TRUE(smi.has_grant("queries.execute.noargs", admin));

  // What query_controller actually asks: the grant that let the request in
  // decides whether arguments are allowed. `queries.execute` is listed first,
  // so a role holding both (the `*` of `full`) keeps passing arguments.
  const session_manager_interface::grant_options both{"queries.execute", "queries.execute.noargs"};
  std::string matched;
  EXPECT_TRUE(smi.can(both, restricted, &matched));
  EXPECT_EQ(matched, "queries.execute.noargs");
  EXPECT_TRUE(smi.can(both, monitoring, &matched));
  EXPECT_EQ(matched, "queries.execute");
  EXPECT_TRUE(smi.can(both, admin, &matched));
  EXPECT_EQ(matched, "queries.execute");
}

TEST(SessionManagerMetricsRole, MetricsGrantsAreWhatTheEndpointsAskFor) {
  // Grants match per dot-separated segment, so the `metrics.get` the
  // `monitoring` role used to carry opened neither metrics endpoint: they are
  // gated by `metrics.list` (/api/v2/metrics) and `openmetrics.list`
  // (/api/v2/openmetrics). The shipped roles now name those two, and the
  // scrape-only `metrics` role holds nothing that runs a command.
  session_manager_interface smi;
  smi.add_user("stale", "stale", "password");
  smi.add_grant("stale", "public,queries.execute,login.get,metrics.get");
  smi.add_user("scraper", "metrics", "password");
  smi.add_grant("metrics", "public,metrics.list,openmetrics.list,login.get");
  smi.add_user("monitor", "monitoring", "password");
  smi.add_grant("monitoring", "public,queries.execute,aliases.list,login.get,metrics.list,openmetrics.list,facts.get");

  Mongoose::StreamResponse stale;
  smi.store_user_in_response("stale", stale);
  EXPECT_FALSE(smi.has_grant("metrics.list", stale));
  EXPECT_FALSE(smi.has_grant("openmetrics.list", stale));

  Mongoose::StreamResponse scraper;
  smi.store_user_in_response("scraper", scraper);
  EXPECT_TRUE(smi.has_grant("metrics.list", scraper));
  EXPECT_TRUE(smi.has_grant("openmetrics.list", scraper));
  EXPECT_FALSE(smi.has_grant("queries.execute", scraper));
  EXPECT_FALSE(smi.has_grant("queries.execute.noargs", scraper));

  Mongoose::StreamResponse monitor;
  smi.store_user_in_response("monitor", monitor);
  EXPECT_TRUE(smi.has_grant("metrics.list", monitor));
  EXPECT_TRUE(smi.has_grant("openmetrics.list", monitor));
  EXPECT_TRUE(smi.has_grant("queries.execute", monitor));
  // Reading the host inventory is part of deciding what to monitor; making
  // every producer collect now is not, so the refresh grant stays with `full`.
  EXPECT_TRUE(smi.has_grant("facts.get", monitor));
  EXPECT_FALSE(smi.has_grant("facts.refresh", monitor));
  EXPECT_FALSE(smi.has_grant("facts.get", scraper));
}

TEST_F(SessionManagerTest, IsAllowedIp) { EXPECT_TRUE(smi.is_allowed("127.0.0.1")); }

TEST_F(SessionManagerTest, RevokeToken) {
  const std::string token = smi.generate_token("user");
  smi.revoke_token(token);
  EXPECT_FALSE(smi.validate_token(token));
}

TEST(SessionManagerAnonymous, AnonymousAccessIsOffByDefault) {
  // Default-off: even when the role is fully wired up (user mapped to role,
  // role configured with a grant), can() must not consult the anonymous
  // grant table without the explicit allow_anonymous flag. We register
  // through tokens directly (bypassing add_grant's refusal gate) so this
  // test exercises the can() gate specifically - not the add_grant gate.
  session_manager_interface smi;
  smi.set_allow_anonymous(true);
  smi.add_user("anonymous", "anonymous", "anonymous");
  smi.add_grant("anonymous", "anything:read");
  // Now flip the flag back off - can() should refuse despite the grant
  // being registered.
  smi.set_allow_anonymous(false);
  Mongoose::StreamResponse resp;
  EXPECT_FALSE(smi.can("anything:read", resp));
}

TEST(SessionManagerAnonymous, AnonymousAccessGrantedOnlyWhenFlagOn) {
  session_manager_interface smi;
  smi.set_allow_anonymous(true);
  // The role grant table is keyed by role name, the user-to-role table is
  // keyed by user name. To resolve `tokens.can("anonymous", ...)` we need
  // both: a user "anonymous" mapped to the role "anonymous", and the role
  // configured with a grant. The naming convention used elsewhere in this
  // codebase is to give the magic user the same name as the magic role.
  smi.add_user("anonymous", "anonymous", "anonymous");
  smi.add_grant("anonymous", "anything:read");
  Mongoose::StreamResponse resp;
  EXPECT_TRUE(smi.can("anything:read", resp));

  // A grant registered for a non-anonymous role still works regardless.
  Mongoose::StreamResponse resp2;
  EXPECT_FALSE(smi.can("other:read", resp2));
}

TEST_F(SessionManagerTest, ReAddingUserRevokesAllTheirTokens) {
  // Re-adding a user (e.g. password rotation through the settings reload path)
  // must invalidate any tokens previously issued to them. Otherwise a stolen
  // bearer token survives a password change.
  const std::string t1 = smi.generate_token("user");
  const std::string t2 = smi.generate_token("user");
  EXPECT_TRUE(smi.validate_token(t1));
  EXPECT_TRUE(smi.validate_token(t2));
  smi.add_user("user", "foo", "newpassword");
  EXPECT_FALSE(smi.validate_token(t1));
  EXPECT_FALSE(smi.validate_token(t2));
}

TEST_F(SessionManagerTest, RateLimiterBlocksAfterRepeatedFailures) {
  Mongoose::Request req("203.0.113.5", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  const std::string bad_auth = "Basic " + Mongoose::Helpers::encode_b64("user:wrong");
  req.get_headers()[HTTP_HDR_AUTH] = bad_auth;

  for (int i = 0; i < 10; ++i) {
    Mongoose::StreamResponse r;
    EXPECT_FALSE(smi.process_auth_header("something:read", req, r));
  }
  // After kDefaultMaxFailures, even a correct password gets rejected from this IP.
  const std::string good_auth = "Basic " + Mongoose::Helpers::encode_b64("user:password");
  req.get_headers()[HTTP_HDR_AUTH] = good_auth;
  EXPECT_FALSE(smi.process_auth_header("something:read", req, resp));
}

TEST_F(SessionManagerTest, Metrics) {
  // The OpenMetrics body is stored and served verbatim - the session manager
  // used to join a list of lines, and is not allowed to reshape the document
  // the renderer produced (dropping its trailing newline would be enough to
  // make the exposition invalid).
  smi.set_metrics("metrics", "{\"cpu\":1}", "{\"cpu\":{\"type\":\"gauge\"}}", "# TYPE open_metrics counter\nopen_metrics_total 1\n# EOF\n",
                  "# TYPE open_metrics_total counter\nopen_metrics_total 1\n# EOF\n");
  EXPECT_EQ(smi.get_metrics(), "metrics");
  // The plain flat list keeps being exactly what it was for everything that
  // does not ask for the metadata; `?meta=1` gets it joined to its metadata.
  EXPECT_EQ(smi.get_metrics_v2(), "{\"cpu\":1}");
  EXPECT_EQ(smi.get_metrics_v2_described(), "{\"metrics\":{\"cpu\":1},\"metadata\":{\"cpu\":{\"type\":\"gauge\"}}}");
  EXPECT_EQ(smi.get_open_metrics(), "# TYPE open_metrics counter\nopen_metrics_total 1\n# EOF\n");
  // The negotiated bodies are separate latches: serving one to a reader that
  // asked for the other loses the type of every counter.
  EXPECT_EQ(smi.get_prometheus_metrics(), "# TYPE open_metrics_total counter\nopen_metrics_total 1\n# EOF\n");
}

TEST_F(SessionManagerTest, LogData) {
  smi.add_log_message(false, {0, 123, "type", "file", "message", "date"});
  EXPECT_NE(smi.get_log_data(), nullptr);
  smi.reset_log();
}

TEST_F(SessionManagerTest, AllowedHosts) {
  smi.set_allowed_hosts("127.0.0.1");
  smi.set_allowed_hosts_cache(true);
  // boot() runs allowed_hosts.refresh() which parses the source string into
  // entries. With cache=true and fail-closed-on-empty (L1), set_source
  // alone is not enough - the entries list stays empty until refresh.
  smi.boot();
  EXPECT_TRUE(smi.is_allowed("127.0.0.1"));
}

TEST_F(SessionManagerTest, ProcessAuthHeaderBasic) {
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  std::string auth = "Basic " + Mongoose::Helpers::encode_b64("user:password");
  req.get_headers()[HTTP_HDR_AUTH] = auth;

  EXPECT_TRUE(smi.process_auth_header("something:read", req, resp));
  EXPECT_EQ(resp.getContext("uid"), "user");
  // Basic auth on an ordinary route authenticates without minting a session.
  EXPECT_TRUE(resp.getContext("token").empty());
}

// ============================================================================
// Legacy query-string auth allowlist
// ============================================================================

TEST(SessionManagerLegacyQueryAuth, DefaultAllowsIcingaCheckNscpApi) {
  // Default-constructed session_manager_interface seeds the allowlist with
  // "Icinga/check_nscp_api" so the bundled plugin keeps working without
  // operator intervention, regardless of version suffix.
  session_manager_interface smi;
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("Icinga/check_nscp_api/2.14.0"));
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("Icinga/check_nscp_api/3.0.0-rc1"));
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("Icinga/check_nscp_api/dev"));
}

TEST(SessionManagerLegacyQueryAuth, DefaultMatchIsCaseInsensitive) {
  session_manager_interface smi;
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("icinga/check_nscp_api/1.0"));
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("ICINGA/CHECK_NSCP_API/2.0"));
}

TEST(SessionManagerLegacyQueryAuth, DefaultRejectsLooseIcingaUserAgents) {
  // The tighter default ("Icinga/check_nscp_api") refuses arbitrary clients
  // that merely mention Icinga somewhere in the UA — those would have slipped
  // through a bare "Icinga" substring default.
  session_manager_interface smi;
  EXPECT_FALSE(smi.client_allows_legacy_query_auth("MyIcingaProbe/1.0"));
  EXPECT_FALSE(smi.client_allows_legacy_query_auth("IcingaWeb2/2.10"));
  EXPECT_FALSE(smi.client_allows_legacy_query_auth("ICINGA-PROBE/1.0"));
}

TEST(SessionManagerLegacyQueryAuth, BrowsersAndGenericClientsAreNotAllowed) {
  session_manager_interface smi;
  EXPECT_FALSE(smi.client_allows_legacy_query_auth("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/120.0"));
  EXPECT_FALSE(smi.client_allows_legacy_query_auth("curl/8.0"));
  EXPECT_FALSE(smi.client_allows_legacy_query_auth(""));
}

TEST(SessionManagerLegacyQueryAuth, EmptyAllowlistRejectsEverything) {
  session_manager_interface smi;
  smi.set_legacy_query_auth_user_agents("");
  EXPECT_FALSE(smi.client_allows_legacy_query_auth("Icinga/check_nscp_api/2.14.0"));
}

TEST(SessionManagerLegacyQueryAuth, MultipleSubstringsAllMatch) {
  session_manager_interface smi;
  smi.set_legacy_query_auth_user_agents("Icinga/check_nscp_api, my-old-probe, scanner");
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("Icinga/check_nscp_api/2.14.0"));
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("my-old-probe/0.9"));
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("CustomScanner/1.0"));
  EXPECT_FALSE(smi.client_allows_legacy_query_auth("Mozilla/5.0"));
}

TEST(SessionManagerLegacyQueryAuth, WhitespaceAroundPatternsIsTrimmed) {
  session_manager_interface smi;
  smi.set_legacy_query_auth_user_agents("  Icinga/check_nscp_api  ,   bot   ");
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("Icinga/check_nscp_api/2.0"));
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("custom-bot/1.0"));
}

TEST(SessionManagerLegacyQueryAuth, OperatorCanLooosenToBareIcinga) {
  // If an operator deploys an Icinga-derived probe that doesn't use the
  // stock check_nscp_api binary name, they can opt back into the looser
  // "Icinga" substring match explicitly via the setting.
  session_manager_interface smi;
  smi.set_legacy_query_auth_user_agents("Icinga");
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("MyIcingaProbe/1.0"));
  EXPECT_TRUE(smi.client_allows_legacy_query_auth("Icinga/check_nscp_api/2.14.0"));
}

TEST_F(SessionManagerTest, ProcessAuthHeaderBearer) {
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  std::string auth = "Bearer validtoken";
  req.get_headers()[HTTP_HDR_AUTH] = auth;

  std::string token = smi.generate_token("user");
  auth = "Bearer " + token;
  req.get_headers()[HTTP_HDR_AUTH] = auth;

  EXPECT_TRUE(smi.process_auth_header("something:read", req, resp));
  EXPECT_EQ(resp.getContext("token"), token);
}

TEST_F(SessionManagerTest, IsLoggedInWithToken) {
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  std::string token = smi.generate_token("user");
  req.get_headers()["TOKEN"] = token;

  EXPECT_TRUE(smi.is_logged_in("something:read", req, resp));
  EXPECT_EQ(resp.getContext("token"), token);
}

// ============================================================================
// Token minting is a login-route decision
//
// process_auth_header used to create an eight-hour token on every Basic or
// `password`-header request, not only on the login routes. Each Icinga
// check_nscp_api poll therefore made one: at the store's 4096-entry cap every
// new login evicts the oldest live entry, so a host running 20 checks a minute
// filled the store in about three and a half hours and then evicted the
// operator's UI session within minutes. Any authenticated user of any role
// could do the same on purpose with 4096 requests.
// ============================================================================

TEST_F(SessionManagerTest, RepeatedBasicAuthDoesNotFillTheTokenStore) {
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  req.get_headers()[HTTP_HDR_AUTH] = "Basic " + Mongoose::Helpers::encode_b64("user:password");

  for (int i = 0; i < 50; i++) {
    Mongoose::StreamResponse resp;
    ASSERT_TRUE(smi.is_logged_in("something:read", req, resp)) << "iteration " << i;
    EXPECT_TRUE(resp.getContext("token").empty()) << "iteration " << i << " minted a session token";
    EXPECT_EQ(resp.getContext("uid"), "user");
  }
}

TEST_F(SessionManagerTest, TheLoginRouteStillMintsAToken) {
  // log_in() is what GET /api/v2/login calls: handing a usable bearer back is
  // the whole purpose of that route.
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  req.get_headers()[HTTP_HDR_AUTH] = "Basic " + Mongoose::Helpers::encode_b64("user:password");

  ASSERT_TRUE(smi.log_in("something:read", req, resp));
  const std::string token = resp.getContext("token");
  ASSERT_FALSE(token.empty());
  EXPECT_TRUE(smi.validate_token(token)) << "the token handed back must be one the store accepts";
}

TEST_F(SessionManagerTest, ThePasswordHeaderDoesNotMintATokenEither) {
  // The Icinga check_nscp_api convention: password only, user implied `admin`.
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  req.get_headers()["password"] = "password";

  smi.add_user("admin", "foo", "password");
  ASSERT_TRUE(smi.is_logged_in("something:read", req, resp)) << "password-header auth should succeed";
  EXPECT_TRUE(resp.getContext("token").empty());
}

// --- Credential fingerprints and session persistence --------------------------
//
// A session is bound to the credentials it was authorised against: the user's
// role plus the password value user_manager stores for them. In memory a
// password or role change revokes through add_user (ReAddingUserRevokesAll
// TheirTokens above); across a restart it is the fingerprint check on import
// that does it, which is what these tests pin. Every live session is exported:
// only log_in() mints a token, and it hands that token to the client.

namespace {
// A password already in PBKDF2 form is stored verbatim, so its fingerprint is
// stable across processes. This is what the first boot writes for `admin`,
// and the only kind of password whose sessions survive a restart.
std::string hashed(const std::string& password) {
  const std::string h = web_password::hash_password(password);
  return h.empty() ? password : h;
}
}  // namespace

TEST(SessionPersistence, ExportedSessionsCarryOnlyHashes) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  session_manager_interface smi;
  smi.add_user("user", "full", hashed("secret"));
  smi.add_grant("full", "*");
  const std::string token = smi.generate_token("user");
  ASSERT_FALSE(token.empty());

  const auto sessions = smi.export_sessions();
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_NE(sessions.front().hash, token) << "the raw token was exported";
  EXPECT_EQ(sessions.front().user, "user");
  EXPECT_FALSE(sessions.front().fingerprint.empty());
}

TEST(SessionPersistence, ImportRestoresASessionForAnUnchangedUser) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");

  session_manager_interface before;
  before.add_user("user", "full", password);
  before.add_grant("full", "*");
  const std::string token = before.generate_token("user");
  ASSERT_FALSE(token.empty());
  const auto sessions = before.export_sessions();
  ASSERT_EQ(sessions.size(), 1u);

  // A second process with the same configuration: the token the client still
  // holds keeps working.
  session_manager_interface after;
  after.add_user("user", "full", password);
  after.add_grant("full", "*");
  EXPECT_FALSE(after.validate_token(token));
  EXPECT_EQ(after.import_sessions(sessions), 1u);
  EXPECT_TRUE(after.validate_token(token));
}

TEST(SessionPersistence, ImportDropsSessionsForAnUnknownUser) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  session_manager_interface before;
  before.add_user("user", "full", hashed("secret"));
  before.add_grant("full", "*");
  const std::string token = before.generate_token("user");
  const auto sessions = before.export_sessions();
  ASSERT_EQ(sessions.size(), 1u);

  // The user was removed from the configuration while the agent was down.
  session_manager_interface after;
  after.add_user("someone-else", "full", hashed("secret"));
  after.add_grant("full", "*");
  EXPECT_EQ(after.import_sessions(sessions), 0u);
  EXPECT_FALSE(after.validate_token(token));
}

TEST(SessionPersistence, ImportDropsSessionsWhoseFingerprintMoved) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  session_manager_interface before;
  before.add_user("user", "full", hashed("secret"));
  before.add_grant("full", "*");
  const std::string token = before.generate_token("user");
  const auto sessions = before.export_sessions();
  ASSERT_EQ(sessions.size(), 1u);

  // Password changed in the INI while the agent was down.
  session_manager_interface changed_password;
  changed_password.add_user("user", "full", hashed("a-different-secret"));
  changed_password.add_grant("full", "*");
  EXPECT_EQ(changed_password.import_sessions(sessions), 0u);
  EXPECT_FALSE(changed_password.validate_token(token));

  // Role changed in the INI while the agent was down.
  session_manager_interface changed_role;
  changed_role.add_user("user", "restricted", hashed("secret"));
  changed_role.add_grant("restricted", "login.get");
  EXPECT_EQ(changed_role.import_sessions(sessions), 0u);
  EXPECT_FALSE(changed_role.validate_token(token));
}

TEST(SessionPersistence, ImportDropsExpiredSessions) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  session_manager_interface before;
  before.add_user("user", "full", password);
  before.add_grant("full", "*");
  const std::string token = before.generate_token("user");
  auto sessions = before.export_sessions();
  ASSERT_EQ(sessions.size(), 1u);
  // The agent was down for longer than a session lives.
  sessions.front().created -= HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS + 1);

  session_manager_interface after;
  after.add_user("user", "full", password);
  after.add_grant("full", "*");
  EXPECT_EQ(after.import_sessions(sessions), 0u);
  EXPECT_FALSE(after.validate_token(token));
}

TEST(SessionPersistence, FingerprintIsAHashOrNothing) {
  // The fingerprint is folded from the role and the stored password value; it
  // must never be that material itself, which in a build without OpenSSL is
  // the cleartext password. No hash function, no fingerprint.
  session_manager_interface smi;
  smi.add_user("user", "full", hashed("secret"));
  const std::string fp = smi.fingerprint_for_user("user");
  if (token_store::has_hashing()) {
    EXPECT_EQ(fp.size(), 64u);
    EXPECT_EQ(fp.find("secret"), std::string::npos);
    EXPECT_EQ(fp.find("full"), std::string::npos);
  } else {
    EXPECT_TRUE(fp.empty());
  }
  EXPECT_TRUE(smi.fingerprint_for_user("nobody").empty());
}

TEST(SessionPersistence, RevokedSessionsAreNotExported) {
  // logout (login_controller::logout) and revoke_tokens_for_user both drop the
  // entry from the live map, and the export reads that map at shutdown - so a
  // revoked session is simply never written back.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  session_manager_interface smi;
  smi.add_user("user", "full", hashed("secret"));
  smi.add_grant("full", "*");
  const std::string token = smi.generate_token("user");
  ASSERT_EQ(smi.export_sessions().size(), 1u);
  smi.revoke_token(token);
  EXPECT_TRUE(smi.export_sessions().empty());
}

// --- Restored sessions on the request path ----------------------------------
//
// The tests above exercise the session table directly. These go through
// is_logged_in with a real Bearer header, which is what a browser tab does
// after the agent came back: the token it kept must authenticate, resolve to
// its user, and carry that user's grants.

namespace {
// A configured agent: one user with a hashed password and a role.
void configure(session_manager_interface& smi, const std::string& password, const std::string& role = "full") {
  smi.add_user("user", role, password);
  smi.add_grant("full", "*");
  smi.add_grant("readonly", "info.get");
  // is_logged_in checks the source address first; boot() parses the source
  // string into entries, as the SessionManagerTest fixture does.
  smi.set_allowed_hosts("127.0.0.1");
  smi.boot();
}

// What the previous process wrote at shutdown: one session for `user`.
std::list<token_store::persisted_session> shutdown_with_one_session(const std::string& password, std::string& token) {
  session_manager_interface before;
  configure(before, password);
  token = before.generate_token("user");
  EXPECT_FALSE(token.empty());
  return before.export_sessions();
}
}  // namespace

TEST(SessionPersistence, RestoredSessionAuthenticatesABearerRequest) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  std::string token;
  const auto sessions = shutdown_with_one_session(password, token);

  session_manager_interface after;
  configure(after, password);
  ASSERT_EQ(after.import_sessions(sessions), 1u);

  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  req.get_headers()[HTTP_HDR_AUTH] = "Bearer " + token;
  EXPECT_TRUE(after.is_logged_in("settings.put", req, resp));
  EXPECT_EQ(resp.getContext("uid"), "user") << "the restored session must resolve to its user";
  EXPECT_EQ(resp.getContext("token"), token);
}

TEST(SessionPersistence, RestoredSessionIsRefusedWithoutTheGrant) {
  // Restoring a session restores identity, not privilege: grants are read
  // from the current configuration, which may have narrowed the role.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  std::string token;
  const auto sessions = shutdown_with_one_session(password, token);

  // Same role name, same password - the fingerprint matches - but the role
  // now grants less.
  session_manager_interface after;
  after.add_user("user", "full", password);
  after.add_grant("full", "info.get");
  after.set_allowed_hosts("127.0.0.1");
  after.boot();
  ASSERT_EQ(after.import_sessions(sessions), 1u);

  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  req.get_headers()[HTTP_HDR_AUTH] = "Bearer " + token;
  Mongoose::StreamResponse ok;
  EXPECT_TRUE(after.is_logged_in("info.get", req, ok));
  Mongoose::StreamResponse refused;
  EXPECT_FALSE(after.is_logged_in("settings.put", req, refused));
}

TEST(SessionPersistence, RestoredSessionCanBeLoggedOut) {
  // DELETE /api/v2/login after a restart: revoke_token with the raw token
  // must find the restored (hash-keyed) entry, and the next shutdown must not
  // write it back.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  std::string token;
  const auto sessions = shutdown_with_one_session(password, token);

  session_manager_interface after;
  configure(after, password);
  ASSERT_EQ(after.import_sessions(sessions), 1u);
  ASSERT_TRUE(after.validate_token(token));
  after.revoke_token(token);
  EXPECT_FALSE(after.validate_token(token));
  EXPECT_TRUE(after.export_sessions().empty());
}

TEST(SessionPersistence, ImportIsIdempotent) {
  // The same table imported twice (a reload that re-read it, say) is one set
  // of sessions, not two, and the second pass restores nothing.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  std::string token;
  const auto sessions = shutdown_with_one_session(password, token);

  session_manager_interface after;
  configure(after, password);
  EXPECT_EQ(after.import_sessions(sessions), 1u);
  EXPECT_EQ(after.import_sessions(sessions), 0u);
  EXPECT_EQ(after.export_sessions().size(), 1u);
}

TEST(SessionPersistence, ImportDropsASessionWithoutAFingerprint) {
  // A row with an empty fingerprint is one no current configuration can
  // vouch for. It must not slip through as "matches nothing, so matches".
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  std::string token;
  auto sessions = shutdown_with_one_session(password, token);
  sessions.front().fingerprint.clear();

  session_manager_interface after;
  configure(after, password);
  EXPECT_EQ(after.import_sessions(sessions), 0u);
  EXPECT_FALSE(after.validate_token(token));
}

TEST(SessionPersistence, ImportKeepsSessionsWrittenWhileTheClockWasAhead) {
  // A host with no battery-backed clock boots at the epoch (or at whatever
  // the last write left behind) and only reaches an NTP server later, so the
  // stored sessions are dated in the future. A future timestamp means
  // "expired" for a live entry, and taking it that way here would drop every
  // session such a host ever stored - and then blank the table at the next
  // shutdown. They are restored and their eight hours start over instead.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  std::string token;
  auto sessions = shutdown_with_one_session(password, token);
  sessions.front().created = token_store::now() + HOURS_TO_SECONDS(24);

  session_manager_interface after;
  configure(after, password);
  EXPECT_EQ(after.import_sessions(sessions), 1u);
  EXPECT_TRUE(after.validate_token(token));
  // Re-dated to this process's clock, not kept in the future: the session
  // expires eight hours from now rather than thirty-two.
  const auto exported = after.export_sessions();
  ASSERT_EQ(exported.size(), 1u);
  EXPECT_LE(exported.front().created, token_store::now());
}

TEST(SessionPersistence, RevokingRunsTheHandlerSoTheTableCanBeWrittenOut) {
  // Logging out has to outlive the process: the module writes the remaining
  // table to disk from this callback, so a kill before the next clean
  // shutdown cannot bring the revoked session back.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  session_manager_interface smi;
  configure(smi, hashed("secret"));
  std::size_t calls = 0;
  std::list<token_store::persisted_session> last_seen;
  smi.set_sessions_revoked_handler([&smi, &calls, &last_seen]() {
    ++calls;
    last_seen = smi.export_sessions();
  });

  const std::string token = smi.generate_token("user");
  ASSERT_FALSE(token.empty());
  EXPECT_EQ(calls, 0u) << "minting a session is not a revocation";

  smi.revoke_token(token);
  EXPECT_EQ(calls, 1u);
  EXPECT_TRUE(last_seen.empty()) << "the handler must see the table without the revoked session";

  const std::string other = smi.generate_token("user");
  smi.revoke_tokens_for_user("user");
  EXPECT_EQ(calls, 2u);
  EXPECT_FALSE(smi.validate_token(other));

  // Removing the handler stops the notifications; nothing else changes.
  smi.set_sessions_revoked_handler(nullptr);
  smi.revoke_token(smi.generate_token("user"));
  EXPECT_EQ(calls, 2u);
}

TEST(SessionPersistence, ImportedSessionsSurviveTheNextShutdown) {
  // Three processes in a row: the token handed out by the first must still
  // work in the third without the client ever logging in again.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  std::string token;
  const auto first_shutdown = shutdown_with_one_session(password, token);

  session_manager_interface second;
  configure(second, password);
  ASSERT_EQ(second.import_sessions(first_shutdown), 1u);
  const auto second_shutdown = second.export_sessions();
  ASSERT_EQ(second_shutdown.size(), 1u);

  session_manager_interface third;
  configure(third, password);
  ASSERT_EQ(third.import_sessions(second_shutdown), 1u);
  EXPECT_TRUE(third.validate_token(token));
}

// --- The credential fingerprint ----------------------------------------------

TEST(SessionPersistence, FingerprintIsStableAcrossProcessesForTheSameConfiguration) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  session_manager_interface a;
  configure(a, password);
  session_manager_interface b;
  configure(b, password);
  EXPECT_EQ(a.fingerprint_for_user("user"), b.fingerprint_for_user("user"));
}

TEST(SessionPersistence, FingerprintTracksPasswordAndRole) {
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  const std::string password = hashed("secret");
  session_manager_interface base;
  configure(base, password);
  session_manager_interface other_password;
  configure(other_password, hashed("other"));
  session_manager_interface other_role;
  configure(other_role, password, "readonly");
  // Different salt, same password: still a different stored value, so a
  // different fingerprint - which is exactly why plaintext INI passwords do
  // not survive a restart.
  session_manager_interface resalted;
  configure(resalted, hashed("secret"));

  const std::string fp = base.fingerprint_for_user("user");
  EXPECT_NE(fp, other_password.fingerprint_for_user("user"));
  EXPECT_NE(fp, other_role.fingerprint_for_user("user"));
  EXPECT_NE(fp, resalted.fingerprint_for_user("user"));
}

TEST(SessionPersistence, PlaintextPasswordsGetADifferentFingerprintEveryProcess) {
  // The documented limitation, pinned: a cleartext INI password is hashed
  // under a fresh salt by each process, so the session it issued does not
  // come back.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  std::string token;
  session_manager_interface before;
  configure(before, "cleartext-secret");
  token = before.generate_token("user");
  const auto sessions = before.export_sessions();

  session_manager_interface after;
  configure(after, "cleartext-secret");
  EXPECT_NE(before.fingerprint_for_user("user"), after.fingerprint_for_user("user"));
  EXPECT_EQ(after.import_sessions(sessions), 0u);
  // ... while the user can of course still log in.
  EXPECT_TRUE(after.validate_user("user", "cleartext-secret"));
}

TEST(SessionPersistence, StoreUserInResponseIssuesASessionWithTheUsersFingerprint) {
  // The production mint path (Basic auth -> store_user_in_response), not the
  // generate_token shortcut: the session it creates carries the fingerprint
  // import will later check against.
  if (!token_store::has_hashing()) GTEST_SKIP() << "build has no hash function";
  session_manager_interface smi;
  configure(smi, hashed("secret"));
  Mongoose::StreamResponse resp;
  ASSERT_TRUE(smi.store_user_in_response("user", resp));
  const std::string token = resp.getContext("token");
  ASSERT_FALSE(token.empty());
  const auto sessions = smi.export_sessions();
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions.front().fingerprint, smi.fingerprint_for_user("user"));
  EXPECT_EQ(sessions.front().user, "user");
}
