/*
 * Unit tests for mcp::string_response.
 */

#include "string_response.hpp"

#include <gtest/gtest.h>

#include <string>

using mcp::string_response;

TEST(StringResponse, DefaultIsEmptyAndZeroCode) {
  string_response r;
  EXPECT_EQ(r.getBody(), "");
  EXPECT_EQ(r.get_response_code(), 0);
}

TEST(StringResponse, ConstructWithCodeAndDataRef) {
  const std::string data = "payload";
  string_response r(HTTP_OK, data);
  EXPECT_EQ(r.get_response_code(), HTTP_OK);
  EXPECT_EQ(r.getBody(), "payload");
}

TEST(StringResponse, ConstructWithCodeRefAndData) {
  const int code = HTTP_NOT_FOUND;
  string_response r(code, std::string{"missing"});
  EXPECT_EQ(r.get_response_code(), HTTP_NOT_FOUND);
  EXPECT_EQ(r.getBody(), "missing");
}

TEST(StringResponse, SetReplacesData) {
  string_response r;
  r.set("first");
  EXPECT_EQ(r.getBody(), "first");
  r.set("second");
  EXPECT_EQ(r.getBody(), "second");
}

TEST(StringResponse, InheritsResponseHeaders) {
  string_response r;
  r.setHeader("X-Test", "1");
  EXPECT_TRUE(r.hasHeader("X-Test"));
}
