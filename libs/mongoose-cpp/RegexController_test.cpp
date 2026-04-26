/*
 * Unit tests for Mongoose::RegexpController.
 */

#include "RegexController.h"

#include <gtest/gtest.h>

#include <boost/regex.hpp>
#include <memory>
#include <string>

#include "RegexRequestHandler.h"
#include "Request.h"
#include "Response.h"
#include "StreamResponse.h"

using Mongoose::RegexpController;
using Mongoose::RegexpRequestHandlerBase;
using Mongoose::Request;
using Mongoose::Response;
using Mongoose::StreamResponse;

namespace {

class FakeHandler : public RegexpRequestHandlerBase {
 public:
  explicit FakeHandler(std::string body) : body(std::move(body)) {}
  Response* process(Request& /*request*/, boost::smatch& what) override {
    last_match_size = what.size();
    auto* r = new StreamResponse();
    r->append(body);
    return r;
  }
  std::string body;
  std::size_t last_match_size = 0;
};

Request make_request(std::string method, std::string url, Request::headers_type headers = {}) {
  return {"127.0.0.1", false, std::move(method), std::move(url), "", std::move(headers), ""};
}

}  // namespace

TEST(RegexpController, HandlesMatchesPrefix) {
  RegexpController c("/api");
  EXPECT_TRUE(c.handles("GET", "/api/anything"));
  EXPECT_TRUE(c.handles("GET", "/api/v1/info"));
  EXPECT_FALSE(c.handles("GET", "/other"));
  EXPECT_FALSE(c.handles("GET", "/"));
}

TEST(RegexpController, GetPrefixReturnsConfiguredPrefix) {
  const RegexpController c("/api");
  EXPECT_EQ(c.get_prefix(), "/api");
}

TEST(RegexpController, SetPrefixUpdatesPrefix) {
  RegexpController c("/api");
  c.setPrefix("/v2");
  EXPECT_EQ(c.get_prefix(), "/v2");
  EXPECT_TRUE(c.handles("GET", "/v2/foo"));
  EXPECT_FALSE(c.handles("GET", "/api/foo"));
}

TEST(RegexpController, GetBaseCombinesHostAndPrefix) {
  const RegexpController c("/api");
  const auto req = make_request("GET", "/api/info", {{"Host", "example.com"}});
  EXPECT_EQ(c.get_base(req), "http://example.com/api");
}

TEST(RegexpController, HandleRequestDispatchesOnRegexMatch) {
  RegexpController c("/api");
  auto* handler = new FakeHandler("matched");
  c.registerRoute("GET", "/items/(\\d+)", handler);

  auto req = make_request("GET", "/api/items/42");
  const std::unique_ptr<Response> response(c.handleRequest(req));
  ASSERT_NE(response, nullptr);
  EXPECT_EQ(dynamic_cast<StreamResponse*>(response.get())->getBody(), "matched");
  // The regex has 1 capture group, so smatch contains 2 entries (whole match + group).
  EXPECT_EQ(handler->last_match_size, 2u);
}

TEST(RegexpController, HandleRequestReturnsErrorResponseOnNoMatch) {
  RegexpController c("/api");
  c.registerRoute("GET", "/items/(\\d+)", new FakeHandler("x"));

  auto req = make_request("GET", "/api/unknown");
  const std::unique_ptr<Response> response(c.handleRequest(req));
  // Should return a "documentMissing" error response, not nullptr.
  ASSERT_NE(response, nullptr);
}

TEST(RegexpController, HandleRequestRespectsHttpVerb) {
  RegexpController c("/api");
  c.registerRoute("POST", "/foo", new FakeHandler("post"));

  auto req = make_request("GET", "/api/foo");
  const std::unique_ptr<Response> response(c.handleRequest(req));
  // GET doesn't match a POST-only route; controller falls through to error.
  ASSERT_NE(response, nullptr);
}

TEST(RegexpController, ValidateArgumentsAcceptsCorrectCount) {
  RegexpController c("/api");
  StreamResponse resp;
  // smatch with N capture groups has size N+1 (the whole-match counts).
  // Use regex_match to populate smatch.
  std::string subject = "abc-42";
  boost::smatch what;
  boost::regex re("([a-z]+)-(\\d+)");
  ASSERT_TRUE(boost::regex_match(subject, what, re));
  ASSERT_EQ(what.size(), 3u);
  EXPECT_TRUE(c.validate_arguments(2, what, resp));
  EXPECT_EQ(resp.getCode(), HTTP_OK);
}

TEST(RegexpController, ValidateArgumentsRejectsWrongCount) {
  RegexpController c("/api");
  StreamResponse resp;
  std::string subject = "abc";
  boost::smatch what;
  boost::regex re("([a-z]+)");
  ASSERT_TRUE(boost::regex_match(subject, what, re));
  EXPECT_FALSE(c.validate_arguments(/*expected*/ 2, what, resp));
  EXPECT_EQ(resp.getCode(), HTTP_BAD_REQUEST);
}
