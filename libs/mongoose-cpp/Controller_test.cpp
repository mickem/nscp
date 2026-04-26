/*
 * Unit tests for Mongoose::Controller (the base class helpers).
 */

#include "Controller.h"

#include <gtest/gtest.h>

#include <memory>

#include "MatchController.h"  // concrete subclass to exercise the base-class methods
#include "Response.h"
#include "StreamResponse.h"

using Mongoose::MatchController;
using Mongoose::Response;
using Mongoose::StreamResponse;

TEST(Controller, ServerInternalErrorReturnsHttp500) {
  const std::unique_ptr<Response> response(Mongoose::MatchController::serverInternalError("something blew up"));
  ASSERT_NE(response, nullptr);
  EXPECT_EQ(response->getCode(), HTTP_SERVER_ERROR);
  auto* body = dynamic_cast<StreamResponse*>(response.get());
  EXPECT_NE(body->getBody().find("something blew up"), std::string::npos);
}

TEST(Controller, DocumentMissingReturnsHttp404) {
  const std::unique_ptr<Response> response(Mongoose::Controller::documentMissing("/path"));
  ASSERT_NE(response, nullptr);
  EXPECT_EQ(response->getCode(), HTTP_NOT_FOUND);
  auto* body = dynamic_cast<StreamResponse*>(response.get());
  EXPECT_NE(body->getBody().find("/path"), std::string::npos);
}
