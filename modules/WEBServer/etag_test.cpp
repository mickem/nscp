// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "etag.hpp"

#include <gtest/gtest.h>

namespace {
Mongoose::Request get_with(const std::string &if_none_match) {
  Mongoose::Request::headers_type headers;
  if (!if_none_match.empty()) headers["If-None-Match"] = if_none_match;
  return Mongoose::Request("127.0.0.1", false, "GET", "/api/v2/facts", "", headers, "");
}
}  // namespace

TEST(ETag, IsAQuotedStableHashOfTheBody) {
  const std::string tag = web_etag::for_body("{\"a\":1}");
  ASSERT_EQ(tag.size(), 18u);
  EXPECT_EQ(tag.front(), '"');
  EXPECT_EQ(tag.back(), '"');
  EXPECT_EQ(tag, web_etag::for_body("{\"a\":1}"));
  EXPECT_NE(tag, web_etag::for_body("{\"a\":2}"));
  // FNV-1a of nothing is its offset basis: pins the function, so a tag a
  // client cached survives an agent upgrade that did not change the body.
  EXPECT_EQ(web_etag::for_body(""), "\"cbf29ce484222325\"");
}

TEST(ETag, MatchesTheWaysAClientCanSpellIt) {
  const std::string tag = "\"0123456789abcdef\"";
  EXPECT_TRUE(web_etag::matches(tag, tag));
  EXPECT_TRUE(web_etag::matches("W/" + tag, tag)) << "If-None-Match compares weakly";
  EXPECT_TRUE(web_etag::matches("\"other\", " + tag, tag));
  EXPECT_TRUE(web_etag::matches("*", tag));
  EXPECT_FALSE(web_etag::matches("", tag));
  EXPECT_FALSE(web_etag::matches("\"other\"", tag));
  EXPECT_FALSE(web_etag::matches("0123456789abcdef", tag)) << "an unquoted tag is not the same tag";
}

TEST(ETag, AFreshRequestGetsTheBodyAndTheTag) {
  Mongoose::Request request = get_with("");
  Mongoose::StreamResponse response;
  web_etag::send(request, response, "{\"a\":1}");
  EXPECT_EQ(response.getCode(), 200);
  EXPECT_EQ(response.getBody(), "{\"a\":1}");
  EXPECT_EQ(response.get_headers()["ETag"], web_etag::for_body("{\"a\":1}"));
  EXPECT_EQ(response.get_headers()["Cache-Control"], "private, no-cache");
  EXPECT_EQ(response.get_headers()["Content-Type"], "application/json");
}

TEST(ETag, ARevalidationThatMatchesGetsA304WithNoBody) {
  Mongoose::Request request = get_with(web_etag::for_body("{\"a\":1}"));
  Mongoose::StreamResponse response;
  web_etag::send(request, response, "{\"a\":1}");
  EXPECT_EQ(response.getCode(), 304);
  EXPECT_EQ(response.getBody(), "");
  // The tag rides the 304 too, and the type stays JSON rather than the
  // server's text/plain default for codes above 299.
  EXPECT_EQ(response.get_headers()["ETag"], web_etag::for_body("{\"a\":1}"));
  EXPECT_EQ(response.get_headers()["Content-Type"], "application/json");
}

TEST(ETag, AStaleTagGetsTheNewBody) {
  Mongoose::Request request = get_with(web_etag::for_body("{\"a\":1}"));
  Mongoose::StreamResponse response;
  web_etag::send(request, response, "{\"a\":2}");
  EXPECT_EQ(response.getCode(), 200);
  EXPECT_EQ(response.getBody(), "{\"a\":2}");
}

TEST(ETag, KeepsAContentTypeTheControllerChose) {
  Mongoose::Request request = get_with("");
  Mongoose::StreamResponse response;
  response.setHeader("Content-Type", "text/plain; version=0.0.4");
  web_etag::send(request, response, "x 1\n");
  EXPECT_EQ(response.get_headers()["Content-Type"], "text/plain; version=0.0.4");
}
