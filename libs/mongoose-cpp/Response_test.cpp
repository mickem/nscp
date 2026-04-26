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
