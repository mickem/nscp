/*
 * Copyright (C) 2004-2026 Michael Medin
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
#include "nsca_ng_client.hpp"

#include <gtest/gtest.h>
#include <openssl/ssl.h>

#include <cstring>
#include <set>
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

// B1 / T1 fix: any unescaped ';' inside a field would corrupt the on-wire
// framing on the receiving Nagios. Verify the canonical case plus a real
// plugin-output style string.
TEST(NscaNgEscape, SemicolonEscaped) {
  EXPECT_EQ(nsca_ng::escape_field("a;b"), "a\\;b");
}

TEST(NscaNgEscape, MultipleSemicolons) {
  EXPECT_EQ(nsca_ng::escape_field("a;b;c"), "a\\;b\\;c");
}

TEST(NscaNgEscape, RealPluginOutputWithSemicolons) {
  EXPECT_EQ(nsca_ng::escape_field("OK: 3 services; all up"), "OK: 3 services\\; all up");
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

// B1 follow-through: building a real command with a semicolon-bearing output
// should produce a properly framed line.
TEST(NscaNgBuildCommand, OutputWithSemicolon) {
  const auto cmd = nsca_ng::build_check_result_command("h", "s", 0, "OK; running", 1L);
  EXPECT_EQ(cmd, "[1] PROCESS_SERVICE_CHECK_RESULT;h;s;0;OK\\; running");
}

TEST(NscaNgBuildCommand, ServiceNameWithSemicolon) {
  // Unusual but legal — make sure a semicolon in a service description
  // doesn't merge into the next field.
  const auto cmd = nsca_ng::build_check_result_command("h", "weird;svc", 0, "ok", 1L);
  EXPECT_EQ(cmd, "[1] PROCESS_SERVICE_CHECK_RESULT;h;weird\\;svc;0;ok");
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

// ============================================================================
// generate_session_id (T4)
// ============================================================================

TEST(NscaNgSessionId, Length) {
  for (int i = 0; i < 32; ++i) {
    const auto id = nsca_ng_client::generate_session_id();
    EXPECT_EQ(id.size(), 8u) << "session id should be 8 base64 chars";
  }
}

TEST(NscaNgSessionId, Alphabet) {
  // Every emitted character must be in the URL-safe base64 alphabet (no
  // padding since the input length is a multiple of 3 bytes).
  const std::string allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (int i = 0; i < 32; ++i) {
    const auto id = nsca_ng_client::generate_session_id();
    for (char c : id) {
      EXPECT_NE(allowed.find(c), std::string::npos) << "unexpected character '" << c << "' in session id " << id;
    }
  }
}

TEST(NscaNgSessionId, Distinct) {
  // 48 bits of entropy — collisions across a few hundred draws should be
  // astronomically unlikely. This test catches a regression where the RNG
  // is bypassed and the ID becomes constant.
  std::set<std::string> ids;
  for (int i = 0; i < 256; ++i) ids.insert(nsca_ng_client::generate_session_id());
  EXPECT_GT(ids.size(), 250u) << "session IDs should be effectively unique";
}

// ============================================================================
// psk_client_cb (T2)
// ============================================================================
//
// We exercise the callback against a real SSL object so the SSL_set_ex_data /
// SSL_get_ex_data path is genuinely covered (the L1 fix). The handshake is
// never started; we only need a valid SSL handle to attach data to.

namespace {
struct openssl_init {
  openssl_init() {
    OPENSSL_init_ssl(0, nullptr);
  }
};
openssl_init g_openssl;

struct ssl_fixture {
  SSL_CTX *ctx = nullptr;
  SSL *ssl = nullptr;
  ssl_fixture() {
    ctx = SSL_CTX_new(TLS_client_method());
    ssl = SSL_new(ctx);
  }
  ~ssl_fixture() {
    if (ssl) SSL_free(ssl);
    if (ctx) SSL_CTX_free(ctx);
  }
};
}  // namespace

TEST(NscaNgPskCallback, FillsIdentityAndPsk) {
  ssl_fixture f;
  nsca_ng_client::psk_credentials creds{"my-id", "secret-pw"};
  SSL_set_ex_data(f.ssl, nsca_ng_client::get_psk_ex_data_index(), &creds);

  char id_buf[32] = {};
  unsigned char psk_buf[32] = {};
  const unsigned int n = nsca_ng_client::psk_client_cb(f.ssl, "hint", id_buf, sizeof(id_buf), psk_buf, sizeof(psk_buf));

  EXPECT_EQ(n, creds.psk.size());
  EXPECT_STREQ(id_buf, "my-id");
  EXPECT_EQ(0, std::memcmp(psk_buf, creds.psk.data(), creds.psk.size()));
}

TEST(NscaNgPskCallback, ZeroIdentityBufferReturnsZero) {
  // L2 fix: prior versions did `max_identity_len - 1` which underflows to
  // SIZE_MAX when max_identity_len is 0 and then the memcpy wrote off the
  // end of a zero-byte buffer. Verify the guard returns cleanly instead.
  ssl_fixture f;
  nsca_ng_client::psk_credentials creds{"x", "y"};
  SSL_set_ex_data(f.ssl, nsca_ng_client::get_psk_ex_data_index(), &creds);

  unsigned char psk_buf[8] = {};
  EXPECT_EQ(0u, nsca_ng_client::psk_client_cb(f.ssl, nullptr, /*identity*/ nullptr, /*max_identity_len*/ 0, psk_buf, sizeof(psk_buf)));

  char id_buf[8] = {};
  EXPECT_EQ(0u, nsca_ng_client::psk_client_cb(f.ssl, nullptr, id_buf, sizeof(id_buf), /*psk*/ nullptr, /*max_psk_len*/ 0));
}

TEST(NscaNgPskCallback, MissingExDataReturnsZero) {
  // No credentials attached at all (e.g. a stray SSL).
  ssl_fixture f;
  char id_buf[8] = {};
  unsigned char psk_buf[8] = {};
  EXPECT_EQ(0u, nsca_ng_client::psk_client_cb(f.ssl, nullptr, id_buf, sizeof(id_buf), psk_buf, sizeof(psk_buf)));
}

TEST(NscaNgPskCallback, IdentityTruncatedToFit) {
  // When the identity is longer than the buffer, the result must be
  // null-terminated and not overflow.
  ssl_fixture f;
  nsca_ng_client::psk_credentials creds{"this-is-a-very-long-identity", "pw"};
  SSL_set_ex_data(f.ssl, nsca_ng_client::get_psk_ex_data_index(), &creds);

  char id_buf[8] = {};  // only 7 chars of identity will fit + NUL
  unsigned char psk_buf[8] = {};
  nsca_ng_client::psk_client_cb(f.ssl, nullptr, id_buf, sizeof(id_buf), psk_buf, sizeof(psk_buf));

  EXPECT_EQ(id_buf[7], '\0');
  EXPECT_EQ(std::string(id_buf), "this-is");
}

// ============================================================================
// connection_data (T3)
// ============================================================================

TEST(NscaNgConnectionData, DefaultsAreApplied) {
  client::destination_container target;
  target.address.host = "host.example";
  client::destination_container sender;
  sender.address.host = "agent01";

  nsca_ng_client::connection_data c(target, sender);

  EXPECT_EQ(c.address, "host.example");
  EXPECT_EQ(c.port_, "5668") << "default NSCA-NG port is 5668";
  EXPECT_TRUE(c.use_psk) << "PSK is the default authentication mode";
  EXPECT_FALSE(c.host_check_default) << "service checks are the default";
  EXPECT_EQ(c.timeout, 30);
  EXPECT_EQ(c.max_output_length, nsca_ng_client::kDefaultMaxOutputBytes);
  // identity defaults to sender hostname
  EXPECT_EQ(c.identity, "agent01");
}

TEST(NscaNgConnectionData, IdentityPreservedWhenSet) {
  client::destination_container target;
  target.address.host = "h";
  target.set_string_data("identity", "explicit-id");
  client::destination_container sender;
  sender.address.host = "agent";

  nsca_ng_client::connection_data c(target, sender);
  EXPECT_EQ(c.identity, "explicit-id");
}

TEST(NscaNgConnectionData, SenderHostOverridesAddress) {
  client::destination_container target;
  target.address.host = "h";
  client::destination_container sender;
  sender.address.host = "agent";
  sender.set_string_data("host", "agent-overridden");

  nsca_ng_client::connection_data c(target, sender);
  EXPECT_EQ(c.sender_hostname, "agent-overridden");
  EXPECT_EQ(c.identity, "agent-overridden") << "identity should default to the resolved sender hostname";
}

TEST(NscaNgConnectionData, HostCheckOptIn) {
  client::destination_container target;
  target.address.host = "h";
  target.set_bool_data("host check", true);
  client::destination_container sender;
  sender.address.host = "a";

  nsca_ng_client::connection_data c(target, sender);
  EXPECT_TRUE(c.host_check_default);
}

TEST(NscaNgConnectionData, MaxOutputLengthOverride) {
  client::destination_container target;
  target.address.host = "h";
  target.set_int_data("max output length", 1024);
  client::destination_container sender;
  sender.address.host = "a";

  nsca_ng_client::connection_data c(target, sender);
  EXPECT_EQ(c.max_output_length, 1024u);
}

TEST(NscaNgConnectionData, DisablePsk) {
  client::destination_container target;
  target.address.host = "h";
  target.set_bool_data("use psk", false);
  client::destination_container sender;
  sender.address.host = "a";

  nsca_ng_client::connection_data c(target, sender);
  EXPECT_FALSE(c.use_psk);
}
