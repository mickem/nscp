/*
 * Unit tests for Mongoose::Request.
 */

#include "Request.h"

#include <gtest/gtest.h>

#include <map>
#include <string>

using Mongoose::Request;

namespace {

Request make_request(std::string method = "GET", std::string url = "/", std::string query = "", Request::headers_type headers = {}, std::string data = "",
                     std::string ip = "127.0.0.1", bool ssl = false) {
  return {std::move(ip), ssl, std::move(method), std::move(url), std::move(query), std::move(headers), std::move(data)};
}

}  // namespace

TEST(Request, BasicGetters) {
  const auto r = make_request("POST", "/api/v1/info", "", {}, "payload", "10.0.0.5", true);
  EXPECT_EQ(r.getMethod(), "POST");
  EXPECT_EQ(r.getUrl(), "/api/v1/info");
  EXPECT_EQ(r.getData(), "payload");
  EXPECT_EQ(r.getRemoteIp(), "10.0.0.5");
  EXPECT_TRUE(r.is_ssl());
}

TEST(Request, HasVariableChecksHeaders) {
  const Request::headers_type h{{"Host", "example.com"}, {"X-Test", "1"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_TRUE(r.hasVariable("Host"));
  EXPECT_TRUE(r.hasVariable("X-Test"));
  EXPECT_FALSE(r.hasVariable("Missing"));
  // Map is case-sensitive on the contained keys.
  EXPECT_FALSE(r.hasVariable("host"));
}

TEST(Request, ReadHeaderReturnsValueOrEmpty) {
  const Request::headers_type h{{"Host", "example.com"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.readHeader("Host"), "example.com");
  EXPECT_EQ(r.readHeader("Missing"), "");
}

TEST(Request, ReadHeaderDoesNotMutateHeaders) {
  const Request::headers_type h{{"Host", "example.com"}};
  auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.readHeader("nope"), "");
  EXPECT_EQ(r.get_headers().size(), 1u);  // No insertion side-effect.
}

TEST(Request, GetHostBuildsCorrectScheme) {
  const Request::headers_type h{{"Host", "example.com"}};
  auto plain = make_request("GET", "/", "", h, "", "127.0.0.1", false);
  EXPECT_EQ(plain.get_host(), "http://example.com");

  auto secure = make_request("GET", "/", "", h, "", "127.0.0.1", true);
  EXPECT_EQ(secure.get_host(), "https://example.com");

  auto missing = make_request("GET", "/");
  EXPECT_EQ(missing.get_host(), "");
}

TEST(Request, GetReturnsQueryValue) {
  const auto r = make_request("GET", "/", "name=alice&age=30");
  EXPECT_EQ(r.get("name"), "alice");
  EXPECT_EQ(r.get("age"), "30");
}

TEST(Request, GetReturnsFallbackWhenMissing) {
  const auto r = make_request("GET", "/", "name=alice");
  EXPECT_EQ(r.get("missing", "fallback"), "fallback");
  EXPECT_EQ(r.get("missing"), "");
}

TEST(Request, GetReturnsFallbackForEmptyQuery) {
  const auto r = make_request("GET", "/", "");
  EXPECT_EQ(r.get("anything", "fb"), "fb");
}

TEST(Request, GetUrlDecodesPercentEncoded) {
  const auto r = make_request("GET", "/", "msg=hello%20world&plus=a%2Bb");
  EXPECT_EQ(r.get("msg"), "hello world");
  EXPECT_EQ(r.get("plus"), "a+b");
}

TEST(Request, GetBoolParsesCommonRepresentations) {
  const auto r = make_request("GET", "/", "yes=true&no=false&mixed=TrUe&num=1");
  EXPECT_TRUE(r.get_bool("yes"));
  EXPECT_FALSE(r.get_bool("no"));
  EXPECT_TRUE(r.get_bool("mixed"));  // case-insensitive
  EXPECT_FALSE(r.get_bool("num"));   // only literal "true" counts
  EXPECT_TRUE(r.get_bool("missing", true));
  EXPECT_FALSE(r.get_bool("missing", false));
}

TEST(Request, GetNumberParsesIntegers) {
  const auto r = make_request("GET", "/", "n=42&neg=-7&bad=abc");
  EXPECT_EQ(r.get_number("n"), 42);
  EXPECT_EQ(r.get_number("neg"), -7);
  EXPECT_EQ(r.get_number("bad", 99), 99);  // fallback on parse failure
  EXPECT_EQ(r.get_number("missing", 5), 5);
}

TEST(Request, GetNumberUsesGivenKeyNotHardcoded) {
  // Regression: get_number used to ignore its key argument and always read "page".
  const auto r = make_request("GET", "/", "limit=10&page=99");
  EXPECT_EQ(r.get_number("limit"), 10);
  EXPECT_EQ(r.get_number("page"), 99);
}

TEST(Request, GetVariablesVectorParsesPairs) {
  const auto r = make_request("GET", "/", "a=1&b=2&c=hello%20world");
  const auto vars = r.getVariablesVector();
  ASSERT_EQ(vars.size(), 3u);
  EXPECT_EQ(vars[0].first, "a");
  EXPECT_EQ(vars[0].second, "1");
  EXPECT_EQ(vars[1].first, "b");
  EXPECT_EQ(vars[1].second, "2");
  EXPECT_EQ(vars[2].first, "c");
  EXPECT_EQ(vars[2].second, "hello world");
}

TEST(Request, GetVariablesVectorEmptyForEmptyQuery) {
  const auto r = make_request("GET", "/", "");
  EXPECT_TRUE(r.getVariablesVector().empty());
}

TEST(Request, GetVariablesVectorHandlesValuelessKey) {
  const auto r = make_request("GET", "/", "flag&k=v");
  const auto vars = r.getVariablesVector();
  ASSERT_EQ(vars.size(), 2u);
  EXPECT_EQ(vars[0].first, "flag");
  EXPECT_EQ(vars[0].second, "");
  EXPECT_EQ(vars[1].first, "k");
  EXPECT_EQ(vars[1].second, "v");
}

TEST(Request, GetCookieReturnsValue) {
  const Request::headers_type h{{"cookie", "session=abc123; theme=dark"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("session"), "abc123");
  EXPECT_EQ(r.getCookie("theme"), "dark");
}

TEST(Request, GetCookieFallbackWhenMissing) {
  const Request::headers_type h{{"cookie", "session=abc"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("missing", "fb"), "fb");
}

TEST(Request, GetCookieFallbackWhenNoCookieHeader) {
  const auto r = make_request("GET", "/");
  EXPECT_EQ(r.getCookie("anything", "fb"), "fb");
}

TEST(Request, GetCookieHandlesQuotedValue) {
  // Note: mg_get_cookie uses space as a delimiter, so values containing spaces
  // are not supported even when quoted. RFC 6265 forbids spaces in values, so
  // we only exercise the quote-stripping behaviour here.
  const Request::headers_type h{{"cookie", "name=\"quoted-value\""}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("name"), "quoted-value");
}

TEST(Request, GetCookieIsCaseInsensitiveForName) {
  // mg_strcasestr is case-insensitive for the variable name.
  Request::headers_type h{{"cookie", "Session=abc"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("session"), "abc");
  EXPECT_EQ(r.getCookie("SESSION"), "abc");
}

TEST(Request, GetCookieDoesNotMutateHeaders) {
  Request::headers_type h{{"Host", "x"}};
  auto r = make_request("GET", "/", "", h);
  (void)r.getCookie("any", "");
  // Should NOT have inserted a "cookie" entry.
  EXPECT_EQ(r.get_headers().size(), 1u);
  EXPECT_EQ(r.get_headers().count("cookie"), 0u);
}

TEST(Request, ConstAccessWorks) {
  const auto r = make_request("GET", "/path", "k=v", {{"Host", "h"}});
  // All these methods must be callable on a const Request.
  EXPECT_EQ(r.getUrl(), "/path");
  EXPECT_EQ(r.getMethod(), "GET");
  EXPECT_TRUE(r.hasVariable("Host"));
  EXPECT_EQ(r.get("k"), "v");
  EXPECT_EQ(r.get_number("missing", 1), 1);
  EXPECT_EQ(r.get_bool("missing", true), true);
  EXPECT_EQ(r.readHeader("Host"), "h");
  EXPECT_EQ(r.getCookie("c", "fb"), "fb");
  EXPECT_FALSE(r.is_ssl());
  const auto& hdrs = r.get_headers();
  EXPECT_EQ(hdrs.size(), 1u);
}
