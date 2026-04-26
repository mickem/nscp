/*
 * Unit tests for Mongoose::MatchController.
 */

#include "MatchController.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "Request.h"
#include "RequestHandler.h"
#include "Response.h"
#include "StreamResponse.h"

using Mongoose::MatchController;
using Mongoose::Request;
using Mongoose::RequestHandlerBase;
using Mongoose::Response;
using Mongoose::StreamResponse;

namespace {

// Minimal handler that records invocation and returns a fixed body.
class FakeHandler : public RequestHandlerBase {
 public:
  explicit FakeHandler(std::string body) : body(std::move(body)) {}
  Response* process(Request& /*request*/) override {
    ++calls;
    auto* r = new StreamResponse();
    r->append(body);
    return r;
  }
  std::string body;
  int calls = 0;
};

Request make_request(std::string method, std::string url) { return {"127.0.0.1", false, std::move(method), std::move(url), "", {}, ""}; }

}  // namespace

TEST(MatchController, HandlesUnknownRouteIsFalse) {
  MatchController c;
  EXPECT_FALSE(c.handles("GET", "/foo"));
}

TEST(MatchController, HandlesRegisteredRoute) {
  MatchController c;
  c.registerRoute("GET", "/foo", new FakeHandler("ok"));
  EXPECT_TRUE(c.handles("GET", "/foo"));
  EXPECT_FALSE(c.handles("POST", "/foo"));  // method must match
  EXPECT_FALSE(c.handles("GET", "/bar"));   // path must match
}

TEST(MatchController, HandleRequestDispatchesToRegisteredHandler) {
  MatchController c;
  auto* handler = new FakeHandler("hello");
  c.registerRoute("GET", "/foo", handler);

  auto req = make_request("GET", "/foo");
  std::unique_ptr<Response> response(c.handleRequest(req));
  ASSERT_NE(response, nullptr);
  EXPECT_EQ(handler->calls, 1);
  EXPECT_EQ(dynamic_cast<StreamResponse*>(response.get())->getBody(), "hello");
}

TEST(MatchController, HandleRequestReturnsNullForUnknownRoute) {
  MatchController c;
  c.registerRoute("GET", "/foo", new FakeHandler("x"));
  auto req = make_request("GET", "/missing");
  const std::unique_ptr<Response> response(c.handleRequest(req));
  EXPECT_EQ(response, nullptr);
}

TEST(MatchController, MultipleRoutesDispatchIndependently) {
  MatchController c;
  auto* a = new FakeHandler("A");
  auto* b = new FakeHandler("B");
  c.registerRoute("GET", "/a", a);
  c.registerRoute("POST", "/b", b);

  auto reqA = make_request("GET", "/a");
  std::unique_ptr<Response> respA(c.handleRequest(reqA));
  ASSERT_NE(respA, nullptr);
  EXPECT_EQ(dynamic_cast<StreamResponse*>(respA.get())->getBody(), "A");
  EXPECT_EQ(a->calls, 1);
  EXPECT_EQ(b->calls, 0);

  auto reqB = make_request("POST", "/b");
  std::unique_ptr<Response> respB(c.handleRequest(reqB));
  ASSERT_NE(respB, nullptr);
  EXPECT_EQ(dynamic_cast<StreamResponse*>(respB.get())->getBody(), "B");
  EXPECT_EQ(b->calls, 1);
}

TEST(MatchController, DestructorDeletesRegisteredHandlers) {
  // Just exercises the destructor path; if handlers aren't deleted, sanitizer
  // builds will flag a leak. Test passes by not crashing / leaking.
  MatchController c;
  c.registerRoute("GET", "/foo", new FakeHandler("x"));
  c.registerRoute("PUT", "/bar", new FakeHandler("y"));
  SUCCEED();
}
