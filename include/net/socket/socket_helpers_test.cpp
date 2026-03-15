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

#include <gtest/gtest.h>

#include <net/socket/server.hpp>
#include <net/socket/socket_helpers.hpp>
#include <string>

// =============================================================================
// socket_exception tests
// =============================================================================

TEST(SocketException, WhatReturnsMessage) {
  socket_helpers::socket_exception ex("test error");
  EXPECT_STREQ(ex.what(), "test error");
}

TEST(SocketException, ReasonReturnsMessage) {
  socket_helpers::socket_exception ex("reason text");
  EXPECT_EQ(ex.reason(), "reason text");
}

TEST(SocketException, CopyConstructor) {
  socket_helpers::socket_exception ex1("copy me");
  socket_helpers::socket_exception ex2(ex1);
  EXPECT_STREQ(ex2.what(), "copy me");
  EXPECT_EQ(ex2.reason(), ex1.reason());
}

TEST(SocketException, EmptyMessage) {
  socket_helpers::socket_exception ex("");
  EXPECT_STREQ(ex.what(), "");
  EXPECT_EQ(ex.reason(), "");
}

// =============================================================================
// server_exception tests
// =============================================================================

TEST(ServerException, WhatReturnsMessage) {
  socket_helpers::server::server_exception ex("server error");
  EXPECT_STREQ(ex.what(), "server error");
}

// =============================================================================
// connection_info default values
// =============================================================================

TEST(ConnectionInfo, DefaultValues) {
  socket_helpers::connection_info info;
  EXPECT_EQ(info.back_log, socket_helpers::connection_info::backlog_default);
  EXPECT_EQ(info.port_, "0");
  EXPECT_EQ(info.thread_pool_size, 0u);
  EXPECT_EQ(info.timeout, 30u);
  EXPECT_EQ(info.retry, 2);
  EXPECT_TRUE(info.reuse);
  EXPECT_TRUE(info.address.empty());
}

TEST(ConnectionInfo, GetPort) {
  socket_helpers::connection_info info;
  info.port_ = "5666";
  EXPECT_EQ(info.get_port(), "5666");
}

TEST(ConnectionInfo, GetIntPort) {
  socket_helpers::connection_info info;
  info.port_ = "5666";
  EXPECT_EQ(info.get_int_port(), 5666);
}

TEST(ConnectionInfo, GetAddress) {
  socket_helpers::connection_info info;
  info.address = "127.0.0.1";
  EXPECT_EQ(info.get_address(), "127.0.0.1");
}

TEST(ConnectionInfo, GetEndpointString) {
  socket_helpers::connection_info info;
  info.address = "192.168.1.1";
  info.port_ = "8080";
  EXPECT_EQ(info.get_endpoint_string(), "192.168.1.1:8080");
}

TEST(ConnectionInfo, GetEndpointStringEmptyAddress) {
  socket_helpers::connection_info info;
  info.port_ = "443";
  EXPECT_EQ(info.get_endpoint_string(), ":443");
}

TEST(ConnectionInfo, GetReuse) {
  socket_helpers::connection_info info;
  EXPECT_TRUE(info.get_reuse());
  info.reuse = false;
  EXPECT_FALSE(info.get_reuse());
}

TEST(ConnectionInfo, CopyConstructor) {
  socket_helpers::connection_info info;
  info.address = "10.0.0.1";
  info.port_ = "1234";
  info.thread_pool_size = 5;
  info.timeout = 60;
  info.retry = 3;
  info.reuse = false;
  info.back_log = 128;

  socket_helpers::connection_info copy(info);
  EXPECT_EQ(copy.address, "10.0.0.1");
  EXPECT_EQ(copy.port_, "1234");
  EXPECT_EQ(copy.thread_pool_size, 5u);
  EXPECT_EQ(copy.timeout, 60u);
  EXPECT_EQ(copy.retry, 3);
  EXPECT_FALSE(copy.reuse);
  EXPECT_EQ(copy.back_log, 128);
}

TEST(ConnectionInfo, AssignmentOperator) {
  socket_helpers::connection_info info;
  info.address = "10.0.0.2";
  info.port_ = "9999";
  info.timeout = 120;

  socket_helpers::connection_info other;
  other = info;
  EXPECT_EQ(other.address, "10.0.0.2");
  EXPECT_EQ(other.port_, "9999");
  EXPECT_EQ(other.timeout, 120u);
}

TEST(ConnectionInfo, ValidateSslDisabled) {
  socket_helpers::connection_info info;
  info.ssl.enabled = false;
  auto errors = info.validate_ssl();
  EXPECT_TRUE(errors.empty());
}

TEST(ConnectionInfo, ValidateCallsValidateSsl) {
  socket_helpers::connection_info info;
  info.ssl.enabled = false;
  auto errors = info.validate();
  EXPECT_TRUE(errors.empty());
}

// =============================================================================
// ssl_opts default values and to_string
// =============================================================================

TEST(SslOpts, DefaultValues) {
  socket_helpers::connection_info::ssl_opts opts;
  EXPECT_FALSE(opts.enabled);
  EXPECT_FALSE(opts.debug_verify);
  EXPECT_EQ(opts.tls_version, "1.2+");
  EXPECT_TRUE(opts.certificate.empty());
  EXPECT_TRUE(opts.certificate_format.empty());
  EXPECT_TRUE(opts.certificate_key.empty());
  EXPECT_TRUE(opts.ca_path.empty());
  EXPECT_TRUE(opts.allowed_ciphers.empty());
  EXPECT_TRUE(opts.dh_key.empty());
  EXPECT_TRUE(opts.verify_mode.empty());
  EXPECT_TRUE(opts.ssl_options.empty());
}

TEST(SslOpts, ToStringDisabled) {
  socket_helpers::connection_info::ssl_opts opts;
  EXPECT_EQ(opts.to_string(), "ssl disabled");
}

TEST(SslOpts, ToStringEnabledNoCert) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.enabled = true;
  opts.verify_mode = "none";
  std::string result = opts.to_string();
  EXPECT_NE(result.find("ssl enabled: none"), std::string::npos);
  EXPECT_NE(result.find("no certificate"), std::string::npos);
}

TEST(SslOpts, ToStringEnabledWithCert) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.enabled = true;
  opts.verify_mode = "peer";
  opts.certificate = "/path/to/cert.pem";
  opts.certificate_format = "PEM";
  opts.certificate_key = "/path/to/key.pem";
  std::string result = opts.to_string();
  EXPECT_NE(result.find("ssl enabled: peer"), std::string::npos);
  EXPECT_NE(result.find("cert: /path/to/cert.pem"), std::string::npos);
  EXPECT_NE(result.find("(PEM)"), std::string::npos);
}

TEST(SslOpts, ToStringDebugVerify) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.enabled = true;
  opts.debug_verify = true;
  std::string result = opts.to_string();
  EXPECT_NE(result.find("debug verify: on"), std::string::npos);
}

TEST(SslOpts, CopyConstructor) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.enabled = true;
  opts.debug_verify = true;
  opts.certificate = "cert.pem";
  opts.certificate_format = "PEM";
  opts.certificate_key = "key.pem";
  opts.ca_path = "/ca";
  opts.allowed_ciphers = "ALL";
  opts.dh_key = "dh.pem";
  opts.verify_mode = "peer";
  opts.tls_version = "1.3";
  opts.ssl_options = "no-sslv2";

  socket_helpers::connection_info::ssl_opts copy(opts);
  EXPECT_TRUE(copy.enabled);
  EXPECT_TRUE(copy.debug_verify);
  EXPECT_EQ(copy.certificate, "cert.pem");
  EXPECT_EQ(copy.certificate_format, "PEM");
  EXPECT_EQ(copy.certificate_key, "key.pem");
  EXPECT_EQ(copy.ca_path, "/ca");
  EXPECT_EQ(copy.allowed_ciphers, "ALL");
  EXPECT_EQ(copy.dh_key, "dh.pem");
  EXPECT_EQ(copy.verify_mode, "peer");
  EXPECT_EQ(copy.tls_version, "1.3");
  EXPECT_EQ(copy.ssl_options, "no-sslv2");
}

TEST(SslOpts, AssignmentOperator) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.enabled = true;
  opts.certificate = "cert.pem";
  opts.tls_version = "1.3";

  socket_helpers::connection_info::ssl_opts other;
  other = opts;
  EXPECT_TRUE(other.enabled);
  EXPECT_EQ(other.certificate, "cert.pem");
  EXPECT_EQ(other.tls_version, "1.3");
}

// =============================================================================
// connection_info to_string
// =============================================================================

TEST(ConnectionInfo, ToStringContainsEndpoint) {
  socket_helpers::connection_info info;
  info.address = "localhost";
  info.port_ = "5666";
  std::string result = info.to_string();
  EXPECT_NE(result.find("address: localhost:5666"), std::string::npos);
}

TEST(ConnectionInfo, ToStringContainsSslInfo) {
  socket_helpers::connection_info info;
  info.ssl.enabled = false;
  std::string result = info.to_string();
  EXPECT_NE(result.find("ssl disabled"), std::string::npos);
}

#ifdef USE_SSL
// =============================================================================
// SSL-specific: get_verify_mode
// =============================================================================

TEST(SslOptsVerifyMode, None) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "none";
  auto mode = opts.get_verify_mode();
  EXPECT_EQ(+mode & +boost::asio::ssl::context_base::verify_none, +boost::asio::ssl::context_base::verify_none);
}

TEST(SslOptsVerifyMode, Peer) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "peer";
  auto mode = opts.get_verify_mode();
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::verify_peer, 0);
}

TEST(SslOptsVerifyMode, FailIfNoCert) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "fail-if-no-cert";
  auto mode = opts.get_verify_mode();
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::verify_fail_if_no_peer_cert, 0);
}

TEST(SslOptsVerifyMode, PeerCert) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "peer-cert";
  auto mode = opts.get_verify_mode();
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::verify_peer, 0);
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::verify_fail_if_no_peer_cert, 0);
}

TEST(SslOptsVerifyMode, ClientOnce) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "client-once";
  auto mode = opts.get_verify_mode();
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::verify_client_once, 0);
}

TEST(SslOptsVerifyMode, Workarounds) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "workarounds";
  auto mode = opts.get_verify_mode();
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::default_workarounds, 0);
}

TEST(SslOptsVerifyMode, Single) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "single";
  // single_dh_use is 0x0 (no-op) on OpenSSL 3.0+, so only assert when non-zero
  auto mode = opts.get_verify_mode();
  if (+boost::asio::ssl::context::single_dh_use != 0) {
    EXPECT_NE(+mode & +boost::asio::ssl::context::single_dh_use, 0);
  }
}

TEST(SslOptsVerifyMode, CommaDelimited) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.verify_mode = "peer,fail-if-no-cert";
  auto mode = opts.get_verify_mode();
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::verify_peer, 0);
  EXPECT_NE(+mode & +boost::asio::ssl::context_base::verify_fail_if_no_peer_cert, 0);
}

// =============================================================================
// SSL-specific: get_tls_min_version
// =============================================================================

TEST(SslOptsTlsMinVersion, Tls13) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tlsv1.3";
  EXPECT_EQ(opts.get_tls_min_version(), TLS1_3_VERSION);
}

TEST(SslOptsTlsMinVersion, Tls13Short) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "1.3";
  EXPECT_EQ(opts.get_tls_min_version(), TLS1_3_VERSION);
}

TEST(SslOptsTlsMinVersion, Tls12) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tlsv1.2";
  EXPECT_EQ(opts.get_tls_min_version(), TLS1_2_VERSION);
}

TEST(SslOptsTlsMinVersion, Tls12WithPlus) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tlsv1.2+";
  EXPECT_EQ(opts.get_tls_min_version(), TLS1_2_VERSION);
}

TEST(SslOptsTlsMinVersion, Tls11) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tls1.1";
  EXPECT_EQ(opts.get_tls_min_version(), TLS1_1_VERSION);
}

TEST(SslOptsTlsMinVersion, Tls10) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tls1.0";
  EXPECT_EQ(opts.get_tls_min_version(), TLS1_VERSION);
}

TEST(SslOptsTlsMinVersion, Sslv3) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "sslv3";
  EXPECT_EQ(opts.get_tls_min_version(), SSL3_VERSION);
}

TEST(SslOptsTlsMinVersion, Ssl3Alias) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "ssl3";
  EXPECT_EQ(opts.get_tls_min_version(), SSL3_VERSION);
}

TEST(SslOptsTlsMinVersion, InvalidThrows) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "invalid";
  EXPECT_THROW(opts.get_tls_min_version(), socket_helpers::socket_exception);
}

TEST(SslOptsTlsMinVersion, CaseInsensitive) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "TLSv1.2";
  EXPECT_EQ(opts.get_tls_min_version(), TLS1_2_VERSION);
}

// =============================================================================
// SSL-specific: get_tls_max_version
// =============================================================================

TEST(SslOptsTlsMaxVersion, Tls13Exact) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tlsv1.3";
  EXPECT_EQ(opts.get_tls_max_version(), TLS1_3_VERSION);
}

TEST(SslOptsTlsMaxVersion, Tls12Plus) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tlsv1.2+";
  EXPECT_EQ(opts.get_tls_max_version(), TLS1_3_VERSION);
}

TEST(SslOptsTlsMaxVersion, Tls12Exact) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tlsv1.2";
  EXPECT_EQ(opts.get_tls_max_version(), TLS1_2_VERSION);
}

TEST(SslOptsTlsMaxVersion, Tls11Exact) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tlsv1.1";
  EXPECT_EQ(opts.get_tls_max_version(), TLS1_1_VERSION);
}

TEST(SslOptsTlsMaxVersion, Tls10Exact) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "tls1.0";
  EXPECT_EQ(opts.get_tls_max_version(), TLS1_VERSION);
}

TEST(SslOptsTlsMaxVersion, Sslv3Plus) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "sslv3+";
  EXPECT_EQ(opts.get_tls_max_version(), TLS1_3_VERSION);
}

TEST(SslOptsTlsMaxVersion, Sslv3Exact) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "sslv3";
  EXPECT_EQ(opts.get_tls_max_version(), SSL3_VERSION);
}

TEST(SslOptsTlsMaxVersion, InvalidThrows) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "garbage";
  EXPECT_THROW(opts.get_tls_max_version(), socket_helpers::socket_exception);
}

TEST(SslOptsTlsMaxVersion, Tls11Plus) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "1.1+";
  EXPECT_EQ(opts.get_tls_max_version(), TLS1_3_VERSION);
}

// =============================================================================
// SSL-specific: get_certificate_format / get_certificate_key_format
// =============================================================================

TEST(SslOptsCertFormat, DefaultIsPem) {
  socket_helpers::connection_info::ssl_opts opts;
  EXPECT_EQ(+opts.get_certificate_format(), +boost::asio::ssl::context::pem);
}

TEST(SslOptsCertFormat, Asn1) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.certificate_format = "asn1";
  EXPECT_EQ(+opts.get_certificate_format(), +boost::asio::ssl::context::asn1);
}

TEST(SslOptsCertFormat, Pem) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.certificate_format = "PEM";
  EXPECT_EQ(+opts.get_certificate_format(), +boost::asio::ssl::context::pem);
}

TEST(SslOptsCertKeyFormat, DefaultIsPem) {
  socket_helpers::connection_info::ssl_opts opts;
  EXPECT_EQ(+opts.get_certificate_key_format(), +boost::asio::ssl::context::pem);
}

TEST(SslOptsCertKeyFormat, Asn1) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.certificate_key_format = "asn1";
  EXPECT_EQ(+opts.get_certificate_key_format(), +boost::asio::ssl::context::asn1);
}

// =============================================================================
// SSL-specific: get_ctx_opts
// =============================================================================

TEST(SslOptsCtxOpts, Empty) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "";
  EXPECT_EQ(opts.get_ctx_opts(), 0);
}

TEST(SslOptsCtxOpts, DefaultWorkarounds) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "default-workarounds";
  EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::default_workarounds, 0);
}

TEST(SslOptsCtxOpts, NoSslv2) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "no-sslv2";
  // SSL_OP_NO_SSLv2 is 0x0 (no-op) on OpenSSL 3.0+, so only assert when non-zero
  if (+boost::asio::ssl::context::no_sslv2 != 0) {
    EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::no_sslv2, 0);
  }
}

TEST(SslOptsCtxOpts, NoSslv3) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "no-sslv3";
  EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::no_sslv3, 0);
}

TEST(SslOptsCtxOpts, NoTlsv1) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "no-tlsv1";
  EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::no_tlsv1, 0);
}

TEST(SslOptsCtxOpts, NoTlsv11) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "no-tlsv1_1";
  EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::no_tlsv1_1, 0);
}

TEST(SslOptsCtxOpts, NoTlsv12) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "no-tlsv1_2";
  EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::no_tlsv1_2, 0);
}

TEST(SslOptsCtxOpts, NoTlsv13) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "no-tlsv1_3";
  EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::no_tlsv1_3, 0);
}

TEST(SslOptsCtxOpts, SingleDhUse) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "single-dh-use";
  // SSL_OP_SINGLE_DH_USE is 0x0 (no-op) on OpenSSL 3.0+, so only assert when non-zero
  if (+boost::asio::ssl::context::single_dh_use != 0) {
    EXPECT_NE(opts.get_ctx_opts() & +boost::asio::ssl::context::single_dh_use, 0);
  }
}

TEST(SslOptsCtxOpts, MultipleCombined) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "default-workarounds,no-sslv3,no-tlsv1";
  long result = opts.get_ctx_opts();
  EXPECT_NE(result & +boost::asio::ssl::context::default_workarounds, 0);
  EXPECT_NE(result & +boost::asio::ssl::context::no_sslv3, 0);
  EXPECT_NE(result & +boost::asio::ssl::context::no_tlsv1, 0);
}

TEST(SslOptsCtxOpts, UnknownOptionIgnored) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.ssl_options = "unknown-option";
  EXPECT_EQ(opts.get_ctx_opts(), 0);
}

// =============================================================================
// SSL-specific: connection_info::get_ctx_opts (delegates to ssl_opts)
// =============================================================================

TEST(ConnectionInfoCtxOpts, DelegatesToSslOpts) {
  socket_helpers::connection_info info;
  info.ssl.ssl_options = "no-sslv3";
  EXPECT_NE(info.get_ctx_opts() & +boost::asio::ssl::context::no_sslv3, 0);
}

// =============================================================================
// SSL-specific: tls_method_parser
// =============================================================================

TEST(TlsMethodParser, Tls13) { EXPECT_EQ(+socket_helpers::tls_method_parser("tlsv1.3"), +boost::asio::ssl::context::tlsv13); }

TEST(TlsMethodParser, Tls13Short) { EXPECT_EQ(+socket_helpers::tls_method_parser("1.3"), +boost::asio::ssl::context::tlsv13); }

TEST(TlsMethodParser, Tls12) { EXPECT_EQ(+socket_helpers::tls_method_parser("tlsv1.2"), +boost::asio::ssl::context::tlsv12); }

TEST(TlsMethodParser, Tls12WithPlus) { EXPECT_EQ(+socket_helpers::tls_method_parser("tlsv1.2+"), +boost::asio::ssl::context::tlsv12); }

TEST(TlsMethodParser, Tls11) { EXPECT_EQ(+socket_helpers::tls_method_parser("tls1.1"), +boost::asio::ssl::context::tlsv11); }

TEST(TlsMethodParser, Tls10) { EXPECT_EQ(+socket_helpers::tls_method_parser("tls1.0"), +boost::asio::ssl::context::tlsv1); }

TEST(TlsMethodParser, Sslv3) { EXPECT_EQ(+socket_helpers::tls_method_parser("sslv3"), +boost::asio::ssl::context::sslv23); }

TEST(TlsMethodParser, Ssl3) { EXPECT_EQ(+socket_helpers::tls_method_parser("ssl3"), +boost::asio::ssl::context::sslv23); }

TEST(TlsMethodParser, CaseInsensitive) { EXPECT_EQ(+socket_helpers::tls_method_parser("TLSv1.2"), +boost::asio::ssl::context::tlsv12); }

TEST(TlsMethodParser, InvalidThrows) { EXPECT_THROW(socket_helpers::tls_method_parser("invalid"), socket_helpers::socket_exception); }

// =============================================================================
// SSL-specific: verify_mode_parser
// =============================================================================

TEST(VerifyModeParser, None) {
  auto mode = socket_helpers::verify_mode_parser("none");
  EXPECT_EQ(+mode & +boost::asio::ssl::verify_none, +boost::asio::ssl::verify_none);
}

TEST(VerifyModeParser, Peer) {
  auto mode = socket_helpers::verify_mode_parser("peer");
  EXPECT_NE(+mode & +boost::asio::ssl::verify_peer, 0);
}

TEST(VerifyModeParser, Certificate) {
  auto mode = socket_helpers::verify_mode_parser("certificate");
  EXPECT_NE(+mode & +boost::asio::ssl::verify_peer, 0);
}

TEST(VerifyModeParser, FailIfNoCert) {
  auto mode = socket_helpers::verify_mode_parser("fail-if-no-cert");
  EXPECT_NE(+mode & +boost::asio::ssl::verify_fail_if_no_peer_cert, 0);
}

TEST(VerifyModeParser, FailIfNoPeerCert) {
  auto mode = socket_helpers::verify_mode_parser("fail-if-no-peer-cert");
  EXPECT_NE(+mode & +boost::asio::ssl::verify_fail_if_no_peer_cert, 0);
}

TEST(VerifyModeParser, ClientCertificate) {
  auto mode = socket_helpers::verify_mode_parser("client-certificate");
  EXPECT_NE(+mode & +boost::asio::ssl::verify_fail_if_no_peer_cert, 0);
}

TEST(VerifyModeParser, PeerCert) {
  auto mode = socket_helpers::verify_mode_parser("peer-cert");
  EXPECT_NE(+mode & +boost::asio::ssl::verify_peer, 0);
  EXPECT_NE(+mode & +boost::asio::ssl::verify_fail_if_no_peer_cert, 0);
}

TEST(VerifyModeParser, InvalidThrows) { EXPECT_THROW(socket_helpers::verify_mode_parser("bogus"), socket_helpers::socket_exception); }

TEST(VerifyModeParser, CommaDelimited) {
  auto mode = socket_helpers::verify_mode_parser("peer,fail-if-no-cert");
  EXPECT_NE(+mode & +boost::asio::ssl::verify_peer, 0);
  EXPECT_NE(+mode & +boost::asio::ssl::verify_fail_if_no_peer_cert, 0);
}

#endif  // USE_SSL
