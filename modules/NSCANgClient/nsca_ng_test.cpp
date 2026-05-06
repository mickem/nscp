/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nsca_ng.hpp"

#include <gtest/gtest.h>

#include <string>

// ============================================================================
// escape_field
// ============================================================================

TEST(NscaNgEscape, PlainTextUnchanged) {
  EXPECT_EQ(nsca_ng::escape_field("normal text"), "normal text");
}

TEST(NscaNgEscape, BackslashDoubled) {
  EXPECT_EQ(nsca_ng::escape_field("a\\b"), "a\\\\b");
}

TEST(NscaNgEscape, NewlineEscaped) {
  EXPECT_EQ(nsca_ng::escape_field("a\nb"), "a\\nb");
}

TEST(NscaNgEscape, BothSpecialChars) {
  EXPECT_EQ(nsca_ng::escape_field("a\\\nb"), "a\\\\\\nb");
}

TEST(NscaNgEscape, EmptyString) {
  EXPECT_EQ(nsca_ng::escape_field(""), "");
}

// ============================================================================
// build_check_result_command
// ============================================================================

TEST(NscaNgBuildCommand, ServiceCheck) {
  const auto cmd = nsca_ng::build_check_result_command("myhost", "myservice", 0, "OK", 1000L);
  EXPECT_EQ(cmd, "[1000] PROCESS_SERVICE_CHECK_RESULT;myhost;myservice;0;OK");
}

TEST(NscaNgBuildCommand, HostCheck) {
  const auto cmd = nsca_ng::build_check_result_command("myhost", "", 1, "DOWN", 2000L);
  EXPECT_EQ(cmd, "[2000] PROCESS_HOST_CHECK_RESULT;myhost;1;DOWN");
}

TEST(NscaNgBuildCommand, ServiceCheckCritical) {
  const auto cmd = nsca_ng::build_check_result_command("srv01", "CPU", 2, "CRIT: load 99%", 3000L);
  EXPECT_EQ(cmd, "[3000] PROCESS_SERVICE_CHECK_RESULT;srv01;CPU;2;CRIT: load 99%");
}

TEST(NscaNgBuildCommand, OutputWithNewlineEscaped) {
  const auto cmd = nsca_ng::build_check_result_command("h", "s", 0, "line1\nline2", 4000L);
  EXPECT_EQ(cmd, "[4000] PROCESS_SERVICE_CHECK_RESULT;h;s;0;line1\\nline2");
}

TEST(NscaNgBuildCommand, HostWithBackslashEscaped) {
  const auto cmd = nsca_ng::build_check_result_command("host\\name", "svc", 0, "ok", 5000L);
  EXPECT_EQ(cmd, "[5000] PROCESS_SERVICE_CHECK_RESULT;host\\\\name;svc;0;ok");
}

// ============================================================================
// build_moin_request
// ============================================================================

TEST(NscaNgMoin, BuildsMoinLine) {
  EXPECT_EQ(nsca_ng::build_moin_request("abc123"), "MOIN 1 abc123");
}

TEST(NscaNgMoin, BuildsMoinLineWithBase64SessionId) {
  EXPECT_EQ(nsca_ng::build_moin_request("A1B2C3D4"), "MOIN 1 A1B2C3D4");
}

// ============================================================================
// build_push_request
// ============================================================================

TEST(NscaNgPush, BuildsPushLine) {
  EXPECT_EQ(nsca_ng::build_push_request(42), "PUSH 42");
}

TEST(NscaNgPush, BuildsPushLineZero) {
  EXPECT_EQ(nsca_ng::build_push_request(0), "PUSH 0");
}

TEST(NscaNgPush, PushLengthIncludesNewline) {
  // The data sent after PUSH is cmd + "\n"; verify that convention is
  // representable.
  const std::string cmd = nsca_ng::build_check_result_command("h", "s", 0, "ok", 1000L);
  const auto len = cmd.size() + 1;  // +1 for trailing '\n'
  EXPECT_EQ(nsca_ng::build_push_request(len), "PUSH " + std::to_string(len));
}

// ============================================================================
// parse_server_response
// ============================================================================

TEST(NscaNgParse, OkayResponse) {
  const auto r = nsca_ng::parse_server_response("OKAY");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::okay);
  EXPECT_TRUE(r.ok());
  EXPECT_EQ(r.message, "");
}

TEST(NscaNgParse, OkayCaseInsensitive) {
  const auto r = nsca_ng::parse_server_response("okay");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::okay);
  EXPECT_TRUE(r.ok());
}

TEST(NscaNgParse, FailResponse) {
  const auto r = nsca_ng::parse_server_response("FAIL bad password");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::fail);
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.message, "bad password");
}

TEST(NscaNgParse, FailNoMessage) {
  const auto r = nsca_ng::parse_server_response("FAIL");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::fail);
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.message, "");
}

TEST(NscaNgParse, BailResponse) {
  const auto r = nsca_ng::parse_server_response("BAIL client disconnected");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::bail);
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.message, "client disconnected");
}

TEST(NscaNgParse, MoinResponse) {
  const auto r = nsca_ng::parse_server_response("MOIN 1");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::moin);
  EXPECT_TRUE(r.ok());
  EXPECT_EQ(r.message, "1");
}

TEST(NscaNgParse, UnknownResponse) {
  const auto r = nsca_ng::parse_server_response("SOMETHING else");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::unknown);
  EXPECT_FALSE(r.ok());
}

TEST(NscaNgParse, EmptyLine) {
  const auto r = nsca_ng::parse_server_response("");
  EXPECT_EQ(r.kind, nsca_ng::server_response::type::unknown);
  EXPECT_FALSE(r.ok());
}
