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

// ---------------------------------------------------------------------------
// Coverage for the form-decoded query parser added in Phase 1 of the
// beast-web-backend swap (replaces mongoose's mg_url_decode /
// mg_http_get_var). These tests pin behaviour parity with mongoose where it
// matters (the parser is reachable from any HTTP query string the
// WEBServer accepts).
// ---------------------------------------------------------------------------

// --- decode_form: the "+" -> space form-url-encoding rule ------------------

TEST(Request, GetDecodesPlusToSpace) {
  // application/x-www-form-urlencoded uses '+' to encode a literal space.
  // Required for parity with mongoose's mg_url_decode(is_form_url_encoded=1).
  const auto r = make_request("GET", "/", "msg=hello+world&q=a+b+c");
  EXPECT_EQ(r.get("msg"), "hello world");
  EXPECT_EQ(r.get("q"), "a b c");
}

TEST(Request, GetDecodesMixedPlusAndPercent) {
  const auto r = make_request("GET", "/", "q=hello+%26+world%21");
  EXPECT_EQ(r.get("q"), "hello & world!");
}

// --- decode_form: hex case-insensitivity -----------------------------------

TEST(Request, GetDecodesLowercaseHex) {
  // Both %2B and %2b must decode to '+'.
  const auto r = make_request("GET", "/", "u=%2b&l=%2B");
  EXPECT_EQ(r.get("u"), "+");
  EXPECT_EQ(r.get("l"), "+");
}

TEST(Request, GetDecodesMixedCaseHex) {
  // "%aB" — mixed case in the same escape.
  const auto r = make_request("GET", "/", "x=%aB");
  EXPECT_EQ(r.get("x"), "\xab");
}

// --- decode_form: malformed-escape handling --------------------------------
//
// The new parser passes through truncated or invalid %XX escapes verbatim
// rather than erroring (mirrors mg_url_decode's behaviour with adequately-
// sized output buffers — the only failure mode in mg_url_decode was "dst too
// small", which we size away).

TEST(Request, GetPassesThroughInvalidPercentEscape) {
  // %ZZ has non-hex digits — must surface literally instead of producing 0.
  const auto r = make_request("GET", "/", "x=a%ZZb&y=%G1");
  EXPECT_EQ(r.get("x"), "a%ZZb");
  EXPECT_EQ(r.get("y"), "%G1");
}

TEST(Request, GetPassesThroughTruncatedPercentEscape) {
  // %X at the very end of input (no second hex digit) and a lone %.
  const auto r = make_request("GET", "/", "a=foo%2&b=bar%");
  EXPECT_EQ(r.get("a"), "foo%2");
  EXPECT_EQ(r.get("b"), "bar%");
}

TEST(Request, GetPassesThroughPercentWithOneValidOneInvalid) {
  // %1Z — first nibble is hex, second isn't. Must passthrough as a unit.
  const auto r = make_request("GET", "/", "x=%1Z");
  EXPECT_EQ(r.get("x"), "%1Z");
}

// --- decode_form: multi-byte UTF-8 round-trip -----------------------------

TEST(Request, GetDecodesUtf8MultibyteEscapes) {
  // "%C3%A9" -> "é" (U+00E9 in UTF-8). The decoder is byte-oriented; the
  // resulting std::string carries the raw bytes 0xC3 0xA9.
  const auto r = make_request("GET", "/", "name=caf%C3%A9");
  const std::string got = r.get("name");
  ASSERT_EQ(got.size(), 5u);  // "caf" + 2 UTF-8 bytes
  EXPECT_EQ(static_cast<unsigned char>(got[3]), 0xC3u);
  EXPECT_EQ(static_cast<unsigned char>(got[4]), 0xA9u);
}

// --- decode_form: key (not just value) decoding ---------------------------

TEST(Request, GetDecodesEncodedKey) {
  // Spec-compliant clients encode reserved chars in keys too. The new
  // parser decodes both sides of the '=' independently.
  const auto r = make_request("GET", "/", "my%20key=value&plus+key=ok");
  EXPECT_EQ(r.get("my key"), "value");
  EXPECT_EQ(r.get("plus key"), "ok");
}

// --- readVariable: scan semantics -----------------------------------------

TEST(Request, GetReturnsFirstOccurrenceForDuplicateKeys) {
  // mg_http_get_var returns the first match; readVariable matches that.
  const auto r = make_request("GET", "/", "k=first&k=second&k=third");
  EXPECT_EQ(r.get("k"), "first");
}

TEST(Request, GetSkipsValuelessPairWhenLookingForKey) {
  // "flag" (no '=') comes before "x=value" in the query string. Looking
  // up "x" must still return "value" — the parser must skip pairs that
  // have no '=' instead of mis-attributing them to a sibling.
  const auto r = make_request("GET", "/", "flag&x=value");
  EXPECT_EQ(r.get("x"), "value");
  EXPECT_EQ(r.get("flag", "fb"), "fb");  // valueless key is not "" — it's absent for get()
}

TEST(Request, GetHandlesEmptyKey) {
  // "=alone" — a leading '=' with no key. readVariable performs an empty-
  // string comparison, so the lookup matches when the caller asks for "".
  const auto r = make_request("GET", "/", "=alone&real=v");
  EXPECT_EQ(r.get(""), "alone");
  EXPECT_EQ(r.get("real"), "v");
}

TEST(Request, GetHandlesEmptyValue) {
  const auto r = make_request("GET", "/", "k=&other=present");
  EXPECT_EQ(r.get("k"), "");
  EXPECT_EQ(r.get("other"), "present");
}

TEST(Request, GetHandlesConsecutiveAmpersands) {
  // "a=1&&b=2" — the empty pair between the &s must not derail subsequent
  // lookups.
  const auto r = make_request("GET", "/", "a=1&&b=2");
  EXPECT_EQ(r.get("a"), "1");
  EXPECT_EQ(r.get("b"), "2");
}

TEST(Request, GetHandlesTrailingAmpersand) {
  const auto r = make_request("GET", "/", "a=1&");
  EXPECT_EQ(r.get("a"), "1");
}

TEST(Request, GetIsCaseSensitiveOnKey) {
  // mg_http_get_var was case-sensitive on the variable name; readVariable
  // is too.
  const auto r = make_request("GET", "/", "Foo=bar");
  EXPECT_EQ(r.get("Foo"), "bar");
  EXPECT_EQ(r.get("foo", "fb"), "fb");
}

// --- get_var_vector: extras over readVariable -----------------------------

TEST(Request, GetVariablesVectorPreservesDuplicateKeys) {
  // Unlike get() (which returns the first hit), getVariablesVector exposes
  // every pair in input order, including duplicates.
  const auto r = make_request("GET", "/", "k=a&k=b&k=c");
  const auto vars = r.getVariablesVector();
  ASSERT_EQ(vars.size(), 3u);
  EXPECT_EQ(vars[0].second, "a");
  EXPECT_EQ(vars[1].second, "b");
  EXPECT_EQ(vars[2].second, "c");
}

TEST(Request, GetVariablesVectorDecodesBothKeyAndValue) {
  const auto r = make_request("GET", "/", "my%20key=hello+world");
  const auto vars = r.getVariablesVector();
  ASSERT_EQ(vars.size(), 1u);
  EXPECT_EQ(vars[0].first, "my key");
  EXPECT_EQ(vars[0].second, "hello world");
}

TEST(Request, GetVariablesVectorPassesThroughInvalidEscapes) {
  // Same passthrough behaviour as readVariable.
  const auto r = make_request("GET", "/", "a=%ZZ&b=%");
  const auto vars = r.getVariablesVector();
  ASSERT_EQ(vars.size(), 2u);
  EXPECT_EQ(vars[0].second, "%ZZ");
  EXPECT_EQ(vars[1].second, "%");
}

// --- Cookie parser edges --------------------------------------------------
//
// mg_get_cookie wasn't touched in Phase 1 but it sits on the same network
// path; tighten coverage on the remaining edge cases that hadn't been
// exercised before.

TEST(Request, GetCookieFirstOfMultiplePicksByName) {
  // Multiple cookies, name-targeted lookup picks the right one regardless
  // of position.
  const Request::headers_type h{{"cookie", "first=A; middle=B; last=C"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("first"), "A");
  EXPECT_EQ(r.getCookie("middle"), "B");
  EXPECT_EQ(r.getCookie("last"), "C");
}

TEST(Request, GetCookieStripsTrailingSemicolon) {
  // mg_get_cookie strips one trailing ';' from the value to avoid leaking
  // the separator. Exercises the p[-1] == ';' branch.
  const Request::headers_type h{{"cookie", "session=abc;"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("session"), "abc");
}

TEST(Request, GetCookieReturnsFallbackForEmptyCookieHeader) {
  // The header exists but is empty — early-out before mg_get_cookie sees it.
  const Request::headers_type h{{"cookie", ""}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("anything", "fb"), "fb");
}

TEST(Request, GetCookieDoesNotMatchSubstringOfOtherCookieName) {
  // The cookie header has "sessionid=X" — asking for "session" must NOT
  // match the prefix. mg_strcasestr finds the substring but mg_get_cookie
  // then requires the next char to be '='.
  const Request::headers_type h{{"cookie", "sessionid=abc"}};
  const auto r = make_request("GET", "/", "", h);
  EXPECT_EQ(r.getCookie("session", "fb"), "fb");
  EXPECT_EQ(r.getCookie("sessionid"), "abc");
}
