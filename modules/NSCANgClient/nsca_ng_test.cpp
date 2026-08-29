// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nsca_ng.hpp"

#include <gtest/gtest.h>
#include <openssl/ssl.h>

#include <boost/asio.hpp>
#include <cstring>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <set>
#include <string>

#include "nsca_ng_client.hpp"

// Provide the NSCAPI singleton so the NSC_TRACE_* / NSC_LOG_* macros that
// reference plugin_singleton resolve at link time. The core_wrapper has null
// function pointers, which makes every log call a harmless no-op — we don't
// exercise the trace path from the test, but the linker still needs the
// symbol because nsca_ng_client.cpp is compiled whole into the test binary.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// ============================================================================
// escape_field
// ============================================================================

TEST(NscaNgEscape, PlainTextUnchanged) { EXPECT_EQ(nsca_ng::escape_field("normal text"), "normal text"); }

TEST(NscaNgEscape, BackslashDoubled) { EXPECT_EQ(nsca_ng::escape_field("a\\b"), "a\\\\b"); }

TEST(NscaNgEscape, NewlineEscaped) { EXPECT_EQ(nsca_ng::escape_field("a\nb"), "a\\nb"); }

TEST(NscaNgEscape, BothSpecialChars) { EXPECT_EQ(nsca_ng::escape_field("a\\\nb"), "a\\\\\\nb"); }

TEST(NscaNgEscape, EmptyString) { EXPECT_EQ(nsca_ng::escape_field(""), ""); }

// B1 / T1 fix: any unescaped ';' inside a field would corrupt the on-wire
// framing on the receiving Nagios. Verify the canonical case plus a real
// plugin-output style string.
TEST(NscaNgEscape, SemicolonEscaped) { EXPECT_EQ(nsca_ng::escape_field("a;b"), "a\\;b"); }

TEST(NscaNgEscape, MultipleSemicolons) { EXPECT_EQ(nsca_ng::escape_field("a;b;c"), "a\\;b\\;c"); }

TEST(NscaNgEscape, RealPluginOutputWithSemicolons) { EXPECT_EQ(nsca_ng::escape_field("OK: 3 services; all up"), "OK: 3 services\\; all up"); }

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

TEST(NscaNgMoin, BuildsMoinLine) { EXPECT_EQ(nsca_ng::build_moin_request("abc123"), "MOIN 1 abc123"); }

TEST(NscaNgMoin, BuildsMoinLineWithBase64SessionId) { EXPECT_EQ(nsca_ng::build_moin_request("A1B2C3D4"), "MOIN 1 A1B2C3D4"); }

// ============================================================================
// build_push_request
// ============================================================================

TEST(NscaNgPush, BuildsPushLine) { EXPECT_EQ(nsca_ng::build_push_request(42), "PUSH 42"); }

TEST(NscaNgPush, BuildsPushLineZero) { EXPECT_EQ(nsca_ng::build_push_request(0), "PUSH 0"); }

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
  openssl_init() { OPENSSL_init_ssl(0, nullptr); }
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
  EXPECT_EQ(c.timeout, 30u);
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

// ============================================================================
// make_ssl_context
// ============================================================================
//
// Regression guard for the cert-mode configuration-ordering fix: SSL_new()
// copies the verify mode, certificate, cipher list and protocol-version
// bounds out of the SSL_CTX at creation time, so the context must be fully
// configured BEFORE the ssl::stream is constructed from it. These tests
// create an SSL object from the context exactly the way the stream
// constructor does and assert the configuration actually arrived on it —
// which is what the previous implementation (configuring the context after
// the stream existed) silently failed to do.

namespace {

nsca_ng_client::connection_data make_cert_mode_data(const std::string &verify_mode, const bool insecure, const std::string &tls_version = "") {
  client::destination_container target;
  target.address.host = "server.example";
  target.set_bool_data("use psk", false);
  if (!verify_mode.empty()) target.set_string_data("verify mode", verify_mode);
  if (insecure) target.set_bool_data("insecure", true);
  if (!tls_version.empty()) target.set_string_data("tls version", tls_version);
  client::destination_container sender;
  sender.address.host = "agent";
  return nsca_ng_client::connection_data(target, sender);
}

// Minimal RAII for an SSL created off the context under test.
struct ssl_from_ctx {
  SSL *ssl = nullptr;
  explicit ssl_from_ctx(boost::asio::ssl::context &ctx) : ssl(SSL_new(ctx.native_handle())) {}
  ~ssl_from_ctx() {
    if (ssl) SSL_free(ssl);
  }
};

}  // namespace

TEST(NscaNgSslContext, CertModeVerifyModeReachesSslObjects) {
  const auto con = make_cert_mode_data("peer-cert", false);
  auto ctx = nsca_ng_client::make_ssl_context(con);
  ssl_from_ctx s(ctx);
  ASSERT_NE(s.ssl, nullptr);
  EXPECT_EQ(SSL_get_verify_mode(s.ssl), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT)
      << "verify mode configured on the context must be copied into SSL objects created from it";
}

TEST(NscaNgSslContext, CertModeTlsVersionBoundsReachSslObjects) {
  // Default floor is tlsv1.2+ (set by connection_data when unspecified).
  const auto con = make_cert_mode_data("peer-cert", false);
  auto ctx = nsca_ng_client::make_ssl_context(con);
  ssl_from_ctx s(ctx);
  ASSERT_NE(s.ssl, nullptr);
  EXPECT_EQ(SSL_get_min_proto_version(s.ssl), TLS1_2_VERSION);
  EXPECT_EQ(SSL_get_max_proto_version(s.ssl), TLS1_3_VERSION);

  const auto con13 = make_cert_mode_data("peer-cert", false, "tlsv1.3");
  auto ctx13 = nsca_ng_client::make_ssl_context(con13);
  ssl_from_ctx s13(ctx13);
  ASSERT_NE(s13.ssl, nullptr);
  EXPECT_EQ(SSL_get_min_proto_version(s13.ssl), TLS1_3_VERSION);
}

TEST(NscaNgSslContext, CertModeUnverifiedRequiresInsecureOptIn) {
  // No verify mode and no insecure flag: the connection would be wide open to
  // MITM, so context construction must refuse.
  EXPECT_THROW(nsca_ng_client::make_ssl_context(make_cert_mode_data("", false)), socket_helpers::socket_exception);
}

TEST(NscaNgSslContext, CertModeInsecureOptInAllowsUnverified) {
  const auto con = make_cert_mode_data("", true);
  auto ctx = nsca_ng_client::make_ssl_context(con);
  ssl_from_ctx s(ctx);
  ASSERT_NE(s.ssl, nullptr);
  EXPECT_EQ(SSL_get_verify_mode(s.ssl), SSL_VERIFY_NONE);
}

TEST(NscaNgSslContext, PskModeContextStaysAtDefaults) {
  // PSK authenticates both ends via the key — no cert verification and no
  // insecure opt-in required; the context must build without throwing.
  client::destination_container target;
  target.address.host = "h";
  client::destination_container sender;
  sender.address.host = "a";
  const nsca_ng_client::connection_data con(target, sender);
  ASSERT_TRUE(con.use_psk);

  auto ctx = nsca_ng_client::make_ssl_context(con);
  ssl_from_ctx s(ctx);
  ASSERT_NE(s.ssl, nullptr);
  EXPECT_EQ(SSL_get_verify_mode(s.ssl), SSL_VERIFY_NONE);
}

TEST(NscaNgConnectionData, ToStringDescribesConnectionWithoutPassword) {
  client::destination_container target;
  target.address.host = "srv.example";
  target.set_string_data("password", "s3cret-pw");
  client::destination_container sender;
  sender.address.host = "agent01";

  nsca_ng_client::connection_data c(target, sender);
  const std::string s = c.to_string();

  EXPECT_NE(s.find("srv.example"), std::string::npos) << s;
  EXPECT_NE(s.find("identity: agent01"), std::string::npos) << s;
  EXPECT_NE(s.find("use_psk: true"), std::string::npos) << s;
  EXPECT_NE(s.find("insecure: false"), std::string::npos) << s;
  EXPECT_NE(s.find("sender: agent01"), std::string::npos) << s;
  EXPECT_NE(s.find("timeout: 30s"), std::string::npos) << s;
  EXPECT_NE(s.find("host_check_default: false"), std::string::npos) << s;
  EXPECT_EQ(s.find("s3cret-pw"), std::string::npos) << "the PSK password must never be traced: " << s;
}

// ============================================================================
// nsca_ng_client_handler — unsupported operations
// ============================================================================

TEST(NscaNgClientHandler, QueryIsNotSupported) {
  nsca_ng_client::nsca_ng_client_handler h;
  PB::Commands::QueryRequestMessage req;
  PB::Commands::QueryResponseMessage resp;
  EXPECT_FALSE(h.query(client::destination_container(), client::destination_container(), req, resp));
}

TEST(NscaNgClientHandler, ExecIsNotSupported) {
  nsca_ng_client::nsca_ng_client_handler h;
  PB::Commands::ExecuteRequestMessage req;
  PB::Commands::ExecuteResponseMessage resp;
  EXPECT_FALSE(h.exec(client::destination_container(), client::destination_container(), req, resp));
}

TEST(NscaNgClientHandler, MetricsIsNotSupported) {
  nsca_ng_client::nsca_ng_client_handler h;
  PB::Metrics::MetricsMessage msg;
  EXPECT_FALSE(h.metrics(client::destination_container(), client::destination_container(), msg));
}

// ============================================================================
// nsca_ng_client_handler::submit — transport-level error paths
// ============================================================================
//
// These drive the full submit pipeline (connection_data extraction, TLS
// connection object setup, retry loop, error reporting) without an NSCA-NG
// server. A local TCP listener (that never speaks TLS) exercises the
// post-connect phases; a freshly closed local port exercises the
// connection-refused path. The MOIN/PUSH exchange itself needs a real
// TLS-PSK server and stays with the integration suite.

namespace {

// A listening socket that accepts TCP connects (via the kernel backlog) but
// never speaks — connects succeed and any TLS handshake stalls forever.
struct local_listener {
  boost::asio::io_context io;
  boost::asio::ip::tcp::acceptor acceptor;
  local_listener() : acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) {}
  unsigned short port() const { return acceptor.local_endpoint().port(); }
};

// Grab a port that was just proven free and close it again, so connecting to
// it is (near-)deterministically refused.
unsigned short closed_port() {
  local_listener l;
  return l.port();
}

client::destination_container ng_target(unsigned short port) {
  client::destination_container t;
  t.set_string_data("address", "127.0.0.1:" + std::to_string(port));
  // Note: "retries" (unlike "retry") lands in the generic data map, which is
  // where the NSCA-NG connection_data reads it from.
  t.set_string_data("retries", "0");
  t.set_string_data("password", "test-psk");
  return t;
}

// Submits a single dummy payload and returns the error message reported in
// the (single) response payload. Asserts the transaction failed.
std::string submit_error(client::destination_container target) {
  nsca_ng_client::nsca_ng_client_handler h;
  client::destination_container sender;
  sender.address.host = "agent01";

  PB::Commands::SubmitRequestMessage req;
  req.add_payload()->set_command("svc");
  PB::Commands::SubmitResponseMessage resp;

  EXPECT_TRUE(h.submit(sender, target, req, resp)) << "submit always produces a response message";
  if (resp.payload_size() != 1) {
    ADD_FAILURE() << "expected exactly one response payload, got " << resp.payload_size();
    return "<no payload>";
  }
  EXPECT_EQ(resp.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  return resp.payload(0).result().message();
}

}  // namespace

TEST(NscaNgSubmit, ConnectionRefusedReportsNetworkError) {
  const std::string err = submit_error(ng_target(closed_port()));
  EXPECT_NE(err.find("NSCA-NG network error"), std::string::npos) << err;
  EXPECT_NE(err.find("connect"), std::string::npos) << err;
}

TEST(NscaNgSubmit, NetworkErrorsAreRetried) {
  // retries=1 → two attempts through the retry loop (with its 1s backoff);
  // both fail on the closed port and the last error is reported.
  client::destination_container t = ng_target(closed_port());
  t.set_string_data("retries", "1");
  const std::string err = submit_error(t);
  EXPECT_NE(err.find("NSCA-NG network error"), std::string::npos) << err;
}

TEST(NscaNgSubmit, BrokenPskCipherListIsReported) {
  // An unparsable cipher list must abort with the actual cause instead of
  // letting OpenSSL fall back to its cert-based default list.
  client::destination_container t = ng_target(closed_port());
  t.set_string_data("allowed ciphers", "NOT-A-REAL-CIPHER");
  const std::string err = submit_error(t);
  EXPECT_NE(err.find("Failed to apply PSK cipher list"), std::string::npos) << err;
}

TEST(NscaNgSubmit, UnresolvableHostIsReported) {
  client::destination_container t = ng_target(1);
  t.address.host = "no such host name";  // spaces: fails resolution without touching DNS
  const std::string err = submit_error(t);
  EXPECT_NE(err.find("Failed to resolve"), std::string::npos) << err;
}

TEST(NscaNgSubmit, CertModeWithoutVerificationIsRefused) {
  // Cert mode + verify none + no insecure opt-in must refuse to tunnel data
  // through an unauthenticated TLS session (MITM protection).
  local_listener server;
  client::destination_container t = ng_target(server.port());
  t.set_string_data("use psk", "false");
  t.set_string_data("verify mode", "none");
  const std::string err = submit_error(t);
  EXPECT_NE(err.find("Refusing to connect"), std::string::npos) << err;
}

TEST(NscaNgSubmit, PskHandshakeAgainstSilentServerTimesOut) {
  // Connect succeeds (kernel backlog) but the server never answers the
  // ClientHello — the deadline must cancel the handshake.
  local_listener server;
  client::destination_container t = ng_target(server.port());
  t.data["timeout"] = "1";  // typed set_string_data("timeout") never reaches the data map
  const std::string err = submit_error(t);
  EXPECT_NE(err.find("TLS handshake timed out"), std::string::npos) << err;
}

TEST(NscaNgSubmit, CertModeInsecureProceedsToHandshake) {
  // With the explicit insecure opt-in the cert-mode connection proceeds past
  // the verification guard (SNI set, handshake started) and then times out
  // against the silent server.
  local_listener server;
  client::destination_container t = ng_target(server.port());
  t.set_string_data("use psk", "false");
  t.set_string_data("verify mode", "none");
  t.set_string_data("insecure", "true");
  t.data["timeout"] = "1";
  const std::string err = submit_error(t);
  EXPECT_NE(err.find("TLS handshake"), std::string::npos) << err;
}
