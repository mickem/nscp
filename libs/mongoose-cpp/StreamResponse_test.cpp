/*
 * Unit tests for Mongoose::StreamResponse.
 */

#include "StreamResponse.h"

#include <gtest/gtest.h>

#include <string>

using Mongoose::StreamResponse;

TEST(StreamResponse, EmptyByDefault) {
  StreamResponse r;
  EXPECT_EQ(r.getBody(), "");
  EXPECT_EQ(r.get_response_code(), 0);
}

TEST(StreamResponse, ConstructorSetsResponseCode) {
  StreamResponse r(HTTP_OK);
  EXPECT_EQ(r.get_response_code(), HTTP_OK);
}

TEST(StreamResponse, AppendConcatenates) {
  StreamResponse r;
  r.append("hello");
  r.append(" ");
  r.append("world");
  EXPECT_EQ(r.getBody(), "hello world");
}

TEST(StreamResponse, WriteAcceptsBinaryWithEmbeddedNulls) {
  StreamResponse r;
  const char buf[] = {'a', '\0', 'b', '\0', 'c'};
  r.write(buf, sizeof(buf));
  const std::string body = r.getBody();
  ASSERT_EQ(body.size(), sizeof(buf));
  EXPECT_EQ(body[0], 'a');
  EXPECT_EQ(body[1], '\0');
  EXPECT_EQ(body[2], 'b');
  EXPECT_EQ(body[3], '\0');
  EXPECT_EQ(body[4], 'c');
}

TEST(StreamResponse, SetCodeServerError) {
  StreamResponse r;
  r.setCodeServerError("oops");
  EXPECT_EQ(r.getCode(), HTTP_SERVER_ERROR);
  EXPECT_EQ(r.getBody(), "oops");
}

TEST(StreamResponse, SetCodeNotFound) {
  StreamResponse r;
  r.setCodeNotFound("missing");
  EXPECT_EQ(r.getCode(), HTTP_NOT_FOUND);
  EXPECT_EQ(r.getBody(), "missing");
}

TEST(StreamResponse, SetCodeForbidden) {
  StreamResponse r;
  r.setCodeForbidden("nope");
  EXPECT_EQ(r.getCode(), HTTP_FORBIDDEN);
  EXPECT_EQ(r.getBody(), "nope");
}

TEST(StreamResponse, SetCodeBadRequest) {
  StreamResponse r;
  r.setCodeBadRequest("bad");
  EXPECT_EQ(r.getCode(), HTTP_BAD_REQUEST);
  EXPECT_EQ(r.getBody(), "bad");
}

TEST(StreamResponse, SetCodeAppendsToExistingBody) {
  StreamResponse r;
  r.append("prefix:");
  r.setCodeServerError("err");
  EXPECT_EQ(r.getBody(), "prefix:err");
}
