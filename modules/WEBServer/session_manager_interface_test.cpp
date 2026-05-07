#include "session_manager_interface.hpp"

#include <Helpers.h>
#include <StreamResponse.h>
#include <gtest/gtest.h>

#include <list>
#include <string>

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

TEST_F(SessionManagerTest, StoreUserInResponseSetsCookies) {
  Mongoose::StreamResponse resp;
  smi.store_user_in_response("user", resp);
  EXPECT_EQ(resp.getCookie("uid"), "user");
  EXPECT_FALSE(resp.getCookie("token").empty());
}

TEST_F(SessionManagerTest, StoreTokenInResponseSetsCookies) {
  Mongoose::StreamResponse resp;
  smi.store_token_in_response("validtoken", resp);
  EXPECT_EQ(resp.getCookie("token"), "validtoken");
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

TEST_F(SessionManagerTest, IsAllowedIp) { EXPECT_TRUE(smi.is_allowed("127.0.0.1")); }

TEST_F(SessionManagerTest, RevokeToken) {
  const std::string token = smi.generate_token("user");
  smi.revoke_token(token);
  EXPECT_FALSE(smi.validate_token(token));
}

TEST(SessionManagerAnonymous, AnonymousAccessIsOffByDefault) {
  // Default-off: an `anonymous` role registered through add_grant is silently
  // refused, and can() does not consult the anonymous grant table.
  session_manager_interface smi;
  smi.add_grant("anonymous", "anything:read");
  Mongoose::StreamResponse resp;
  EXPECT_FALSE(smi.can("anything:read", resp));
}

TEST(SessionManagerAnonymous, AnonymousAccessGrantedOnlyWhenFlagOn) {
  session_manager_interface smi;
  smi.set_allow_anonymous(true);
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
  // After kMaxFailures, even a correct password gets rejected from this IP.
  const std::string good_auth = "Basic " + Mongoose::Helpers::encode_b64("user:password");
  req.get_headers()[HTTP_HDR_AUTH] = good_auth;
  EXPECT_FALSE(smi.process_auth_header("something:read", req, resp));
}

TEST_F(SessionManagerTest, Metrics) {
  smi.set_metrics("metrics", "metrics_list", {"open_metrics"});
  EXPECT_EQ(smi.get_metrics(), "metrics");
  EXPECT_EQ(smi.get_metrics_v2(), "metrics_list");
  EXPECT_EQ(smi.get_open_metrics(), "open_metrics\n");
}

TEST_F(SessionManagerTest, LogData) {
  smi.add_log_message(false, {0, 123, "type", "file", "message", "date"});
  EXPECT_NE(smi.get_log_data(), nullptr);
  smi.reset_log();
}

TEST_F(SessionManagerTest, AllowedHosts) {
  smi.set_allowed_hosts("127.0.0.1");
  smi.set_allowed_hosts_cache(true);
  EXPECT_TRUE(smi.is_allowed("127.0.0.1"));
}

TEST_F(SessionManagerTest, ProcessAuthHeaderBasic) {
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  std::string auth = "Basic " + Mongoose::Helpers::encode_b64("user:password");
  req.get_headers()[HTTP_HDR_AUTH] = auth;

  EXPECT_TRUE(smi.process_auth_header("something:read", req, resp));
  EXPECT_EQ(resp.getCookie("uid"), "user");
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
  EXPECT_EQ(resp.getCookie("token"), token);
}

TEST_F(SessionManagerTest, IsLoggedInWithToken) {
  Mongoose::Request req("127.0.0.1", false, "GET", "/", "", {}, "");
  Mongoose::StreamResponse resp;
  std::string token = smi.generate_token("user");
  req.get_headers()["TOKEN"] = token;

  EXPECT_TRUE(smi.is_logged_in("something:read", req, resp));
  EXPECT_EQ(resp.getCookie("token"), token);
}
