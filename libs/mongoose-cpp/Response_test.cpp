/*
 * Unit tests for Mongoose::Response (via the concrete StreamResponse).
 */

#include "Response.h"

#include <gtest/gtest.h>

#include "StreamResponse.h"

using Mongoose::Response;
using Mongoose::StreamResponse;

TEST(Response, DefaultsToHttpOk) {
  const StreamResponse r;
  EXPECT_EQ(r.getCode(), HTTP_OK);
}

TEST(Response, SetCodeUpdatesCode) {
  StreamResponse r;
  r.setCode(HTTP_NOT_FOUND, REASON_NOT_FOUND);
  EXPECT_EQ(r.getCode(), HTTP_NOT_FOUND);
}

TEST(Response, SetCodeOkResetsToOk) {
  StreamResponse r;
  r.setCode(HTTP_SERVER_ERROR, REASON_SERVER_ERROR);
  r.setCodeOk();
  EXPECT_EQ(r.getCode(), HTTP_OK);
}

TEST(Response, HeaderRoundTrip) {
  StreamResponse r;
  EXPECT_FALSE(r.hasHeader("Content-Type"));
  r.setHeader("Content-Type", "application/json");
  EXPECT_TRUE(r.hasHeader("Content-Type"));
  EXPECT_EQ(r.get_headers().at("Content-Type"), "application/json");
}

TEST(Response, SetHeaderOverwrites) {
  StreamResponse r;
  r.setHeader("X", "1");
  r.setHeader("X", "2");
  EXPECT_EQ(r.get_headers().at("X"), "2");
}

TEST(Response, SetHeaderStripsCrLfFromValue) {
  // CR / LF in a header value would be interpreted by a downstream HTTP
  // serializer as a header boundary and let a caller smuggle extra headers
  // (response splitting). The sanitiser strips them.
  StreamResponse r;
  r.setHeader("Link", "</api>; rel=\"next\"\r\nSet-Cookie: evil=1");
  const std::string &v = r.get_headers().at("Link");
  EXPECT_EQ(v.find('\r'), std::string::npos);
  EXPECT_EQ(v.find('\n'), std::string::npos);
  // Surrounding bytes survive.
  EXPECT_NE(v.find("</api>"), std::string::npos);
  EXPECT_NE(v.find("Set-Cookie"), std::string::npos);  // present but no longer a separate header
}

TEST(Response, SetHeaderStripsNulFromValue) {
  StreamResponse r;
  r.setHeader("X-Test", std::string("abc\0def", 7));
  EXPECT_EQ(r.get_headers().at("X-Test"), "abcdef");
}

TEST(Response, SetHeaderStripsControlCharsFromKey) {
  // Header keys are equally dangerous if they contain CR/LF/colon/space -
  // a key like "X\r\nEvil" would inject a new header line.
  StreamResponse r;
  r.setHeader("X-Custom\r\nInjected: bad", "value");
  EXPECT_FALSE(r.hasHeader("X-Custom\r\nInjected: bad"));
  EXPECT_TRUE(r.hasHeader("X-CustomInjectedbad"));
}

TEST(Response, CookieRoundTrip) {
  StreamResponse r;
  r.setCookie("session", "abc");
  EXPECT_EQ(r.getCookie("session"), "abc");
  EXPECT_EQ(r.getCookie("missing"), "");
}

TEST(Response, CookieDoesNotPolluteHeaders) {
  StreamResponse r;
  r.setCookie("session", "abc");
  EXPECT_FALSE(r.hasHeader("session"));
  EXPECT_TRUE(r.get_headers().empty());
}

TEST(Response, CookieDefaultAttributesAreSecureAndHttpOnly) {
  StreamResponse r;
  r.setCookie("session", "abc");
  const auto &cookies = r.get_cookies();
  ASSERT_EQ(cookies.count("session"), 1u);
  const auto &attrs = cookies.at("session").second;
  EXPECT_TRUE(attrs.http_only);
  EXPECT_TRUE(attrs.secure);
  EXPECT_EQ(attrs.same_site, "Strict");
  EXPECT_EQ(attrs.path, "/");
  EXPECT_LT(attrs.max_age, 0);
}

TEST(Response, CookieAttributesOverloadStoresValues) {
  StreamResponse r;
  Response::cookie_attrs a;
  a.http_only = false;
  a.secure = false;
  a.same_site = "Lax";
  a.path = "/api";
  a.max_age = 3600;
  r.setCookie("token", "xyz", a);
  EXPECT_EQ(r.getCookie("token"), "xyz");
  const auto &attrs = r.get_cookies().at("token").second;
  EXPECT_FALSE(attrs.http_only);
  EXPECT_FALSE(attrs.secure);
  EXPECT_EQ(attrs.same_site, "Lax");
  EXPECT_EQ(attrs.path, "/api");
  EXPECT_EQ(attrs.max_age, 3600);
}
