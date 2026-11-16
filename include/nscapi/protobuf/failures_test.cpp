// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/protobuf/failures.hpp>
#include <string>

namespace f = nscapi::failures;
using Command = PB::Failures::FailuresRequestMessage::Request;

namespace {
PB::Failures::FailuresRequestMessage::Request decode(const std::string &wire) {
  PB::Failures::FailuresRequestMessage message;
  EXPECT_TRUE(message.ParseFromString(wire));
  EXPECT_EQ(message.payload_size(), 1);
  return message.payload(0);
}
}  // namespace

// A plain GET asks for everything: `since` and `limit` stay unset rather
// than being sent as explicit zeros.
TEST(Failures, MakeRequestDefaultsToEverything) {
  const auto r = decode(f::make_request(Command::GET));
  EXPECT_EQ(r.command(), Command::GET);
  EXPECT_EQ(r.since(), 0u);
  EXPECT_EQ(r.limit(), 0u);
  EXPECT_EQ(r.index(), 0u);
  EXPECT_FALSE(r.include_context());
}

TEST(Failures, MakeRequestCarriesSinceAndLimit) {
  const auto r = decode(f::make_request(Command::GET, 41, 10));
  EXPECT_EQ(r.since(), 41u);
  EXPECT_EQ(r.limit(), 10u);
  EXPECT_EQ(decode(f::make_request(Command::CLEAR)).command(), Command::CLEAR);
}

TEST(Failures, MakeEntryRequest) {
  const auto with = decode(f::make_entry_request(7, true));
  EXPECT_EQ(with.command(), Command::GET);
  EXPECT_EQ(with.index(), 7u);
  EXPECT_TRUE(with.include_context());
  EXPECT_FALSE(decode(f::make_entry_request(7, false)).include_context());
}

// These words are part of the REST and fleet-report wire format.
TEST(Failures, ContextStateNames) {
  using M = PB::Failures::MetricsContext;
  EXPECT_STREQ(f::context_state_name(M::CAPTURED), "captured");
  EXPECT_STREQ(f::context_state_name(M::EVICTED), "evicted");
  EXPECT_STREQ(f::context_state_name(M::TOO_LARGE), "too_large");
  EXPECT_STREQ(f::context_state_name(M::DISABLED), "disabled");
  EXPECT_STREQ(f::context_state_name(M::NONE), "none");

  using D = PB::Failures::DiagnosticsContext;
  EXPECT_STREQ(f::context_state_name(D::CAPTURED), "captured");
  EXPECT_STREQ(f::context_state_name(D::EVICTED), "evicted");
  EXPECT_STREQ(f::context_state_name(D::TOO_LARGE), "too_large");
  EXPECT_STREQ(f::context_state_name(D::DISABLED), "disabled");
  EXPECT_STREQ(f::context_state_name(D::NONE), "none");
}

TEST(Failures, StatusName) {
  EXPECT_STREQ(f::status_name(PB::Common::ResultCode::OK), "OK");
  EXPECT_STREQ(f::status_name(PB::Common::ResultCode::WARNING), "WARNING");
  EXPECT_STREQ(f::status_name(PB::Common::ResultCode::CRITICAL), "CRITICAL");
  EXPECT_STREQ(f::status_name(PB::Common::ResultCode::UNKNOWN), "UNKNOWN");
  EXPECT_STREQ(f::status_name(static_cast<PB::Common::ResultCode>(42)), "UNKNOWN");
}
