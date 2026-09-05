// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <net/socket/server.hpp>
#include <net/socket/socket_helpers.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <str/utils.hpp>
#include <string>
#include <vector>
#ifndef WIN32
#include <sys/stat.h>
#endif

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

// =============================================================================
// expand_hostname
// =============================================================================

TEST(ExpandHostname, AutoReturnsSystemHostname) {
  std::string host = boost::asio::ip::host_name();
  EXPECT_EQ(socket_helpers::expand_hostname("auto"), host);
}

TEST(ExpandHostname, AutoLcLowercases) {
  std::string host = boost::asio::ip::host_name();
  std::string expected = boost::algorithm::to_lower_copy(host);
  EXPECT_EQ(socket_helpers::expand_hostname("auto-lc"), expected);
}

TEST(ExpandHostname, AutoUcUppercases) {
  std::string host = boost::asio::ip::host_name();
  std::string expected = boost::algorithm::to_upper_copy(host);
  EXPECT_EQ(socket_helpers::expand_hostname("auto-uc"), expected);
}

TEST(ExpandHostname, LiteralPassthroughWithoutPlaceholders) { EXPECT_EQ(socket_helpers::expand_hostname("static-name"), "static-name"); }

TEST(ExpandHostname, EmptyPassthrough) { EXPECT_EQ(socket_helpers::expand_hostname(""), ""); }

TEST(ExpandHostname, HostPlaceholderIsExpanded) {
  std::string out = socket_helpers::expand_hostname("prefix-${host}-suffix");
  EXPECT_EQ(out.find("${host}"), std::string::npos);
  EXPECT_NE(out.find("prefix-"), std::string::npos);
  EXPECT_NE(out.find("-suffix"), std::string::npos);
}

TEST(ExpandHostname, HostnamePlaceholderIsTheFullSystemName) {
  // ${host} stops at the first '.', ${hostname} does not - that is the whole
  // point of having both.
  const std::string host = boost::asio::ip::host_name();
  EXPECT_EQ(socket_helpers::expand_hostname("${hostname}"), host);
  EXPECT_EQ(socket_helpers::expand_hostname("a=${hostname}&b=1"), "a=" + host + "&b=1");
}

TEST(ExpandHostname, HostnameCasePlaceholders) {
  const std::string host = boost::asio::ip::host_name();
  EXPECT_EQ(socket_helpers::expand_hostname("${hostname_lc}"), boost::algorithm::to_lower_copy(host));
  EXPECT_EQ(socket_helpers::expand_hostname("${hostname_uc}"), boost::algorithm::to_upper_copy(host));
}

TEST(ExpandHostname, HostnameAndHostPlaceholdersDoNotCollide) {
  // "${host}" is a character-wise prefix of "${hostname}" up to the brace, so a
  // careless replace order would rewrite "${hostname}" into "<host>name}".
  const std::string host = boost::asio::ip::host_name();
  const std::string out = socket_helpers::expand_hostname("${hostname}|${host}|${hostname_lc}|${host_lc}");
  EXPECT_EQ(out.find("${"), std::string::npos) << out;
  EXPECT_EQ(out.find("name}"), std::string::npos) << out;
  EXPECT_EQ(out.substr(0, host.size()), host) << out;
}

TEST(ExpandHostname, CasePlaceholdersAreExpanded) {
  // No assertion on the exact host name (varies per machine), only that all
  // placeholders are substituted away.
  std::string out = socket_helpers::expand_hostname("${host_lc}|${host_uc}|${domain_lc}|${domain_uc}|${domain}");
  EXPECT_EQ(out.find("${"), std::string::npos);
}

// =============================================================================
// format_ipv6
//
// The pure formatting half of the ${address_ipv6*} placeholders (#349). The
// address discovery half is machine-dependent and covered below only as far as
// determinism allows.
// =============================================================================

TEST(FormatIpv6, CompressedLowercaseIsRfc5952) {
  const auto addr = boost::asio::ip::make_address_v6("2001:0db8:0000:2000:0000:0000:0000:0007");
  EXPECT_EQ(socket_helpers::format_ipv6(addr, false, true), "2001:db8:0:2000::7");
}

TEST(FormatIpv6, CompressedUppercase) {
  const auto addr = boost::asio::ip::make_address_v6("2001:0db8:0000:2000:0000:0000:0000:0007");
  EXPECT_EQ(socket_helpers::format_ipv6(addr, true, true), "2001:DB8:0:2000::7");
}

TEST(FormatIpv6, UncompressedLowercaseIsFullyPadded) {
  const auto addr = boost::asio::ip::make_address_v6("2001:db8:0:2000::7");
  EXPECT_EQ(socket_helpers::format_ipv6(addr, false, false), "2001:0db8:0000:2000:0000:0000:0000:0007");
}

TEST(FormatIpv6, UncompressedUppercase) {
  const auto addr = boost::asio::ip::make_address_v6("2001:db8:0:2000::7");
  EXPECT_EQ(socket_helpers::format_ipv6(addr, true, false), "2001:0DB8:0000:2000:0000:0000:0000:0007");
}

TEST(FormatIpv6, LoopbackBothForms) {
  const auto addr = boost::asio::ip::make_address_v6("::1");
  EXPECT_EQ(socket_helpers::format_ipv6(addr, false, true), "::1");
  EXPECT_EQ(socket_helpers::format_ipv6(addr, false, false), "0000:0000:0000:0000:0000:0000:0000:0001");
}

// =============================================================================
// ${address_*} placeholder expansion
//
// The resolved value is machine-dependent (and a machine may have no address
// in a family at all, in which case the token is deliberately left alone), so
// these assert the shape of the substitution, not a specific address.
// =============================================================================

namespace {
bool looks_like_ipv4(const std::string& value) {
  boost::system::error_code ec;
  boost::asio::ip::make_address_v4(value, ec);
  return !ec;
}
bool looks_like_ipv6(const std::string& value) {
  boost::system::error_code ec;
  boost::asio::ip::make_address_v6(value, ec);
  return !ec;
}
}  // namespace

TEST(ExpandHostname, AddressIpv4IsAnAddressOrLeftAlone) {
  const std::string out = socket_helpers::expand_hostname("ip-${address_ipv4}-end");
  if (out == "ip-${address_ipv4}-end") return;  // no usable IPv4 on this machine
  ASSERT_EQ(out.substr(0, 3), "ip-");
  ASSERT_EQ(out.substr(out.size() - 4), "-end");
  const std::string value = out.substr(3, out.size() - 7);
  EXPECT_TRUE(looks_like_ipv4(value)) << value;
}

TEST(ExpandHostname, AddressIpv6VariantsAgree) {
  const std::string out = socket_helpers::expand_hostname("${address_ipv6}|${address_ipv6_lc}|${address_ipv6_lc_comp}");
  if (out == "${address_ipv6}|${address_ipv6_lc}|${address_ipv6_lc_comp}") return;  // no usable IPv6 on this machine
  // All three spell the same canonical (lowercase, compressed) form.
  std::vector<std::string> parts;
  boost::algorithm::split(parts, out, boost::algorithm::is_any_of("|"));
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[0], parts[1]);
  EXPECT_EQ(parts[0], parts[2]);
  EXPECT_TRUE(looks_like_ipv6(parts[0])) << parts[0];
  EXPECT_EQ(parts[0], boost::algorithm::to_lower_copy(parts[0]));
}

TEST(ExpandHostname, AddressIpv6CaseAndPaddingVariants) {
  const std::string out = socket_helpers::expand_hostname("${address_ipv6_lc}|${address_ipv6_uc}|${address_ipv6_lc_uncomp}|${address_ipv6_uc_uncomp}");
  if (out.find("${") != std::string::npos) {
    // No usable IPv6: every token must have been left alone, not just some.
    EXPECT_EQ(out, "${address_ipv6_lc}|${address_ipv6_uc}|${address_ipv6_lc_uncomp}|${address_ipv6_uc_uncomp}");
    return;
  }
  std::vector<std::string> parts;
  boost::algorithm::split(parts, out, boost::algorithm::is_any_of("|"));
  ASSERT_EQ(parts.size(), 4u);
  EXPECT_EQ(parts[1], boost::algorithm::to_upper_copy(parts[0]));
  EXPECT_EQ(parts[3], boost::algorithm::to_upper_copy(parts[2]));
  // The uncompressed form is always eight fully padded groups.
  EXPECT_EQ(parts[2].size(), 39u) << parts[2];
  EXPECT_TRUE(looks_like_ipv6(parts[2])) << parts[2];
  // ...and both forms name the same address.
  EXPECT_EQ(boost::asio::ip::make_address_v6(parts[0]), boost::asio::ip::make_address_v6(parts[2]));
}

TEST(ExpandHostnamePlaceholders, AddressTokensAreExpandedWithoutHostTokens) {
  // The cheap "does the spec have any placeholder at all" gate must not skip a
  // spec that only carries address tokens.
  const std::string out = socket_helpers::expand_hostname_placeholders("${address_ipv4}");
  if (out != "${address_ipv4}") EXPECT_TRUE(looks_like_ipv4(out)) << out;
}

// =============================================================================
// expand_hostname_placeholders
//
// The half of expand_hostname which is applied to strings that are not host
// name specs - settings contexts and attachment paths (issue #458). It must
// substitute exactly what expand_hostname substitutes and nothing more.
// =============================================================================

TEST(ExpandHostnamePlaceholders, SubstitutesTheSamePlaceholders) {
  const std::string spec = "${hostname}|${host}|${domain}|${hostname_lc}|${host_uc}|${domain_lc}";
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders(spec), socket_helpers::expand_hostname(spec));
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders(spec).find("${"), std::string::npos);
}

TEST(ExpandHostnamePlaceholders, LeavesTheAutoShorthandsAlone) {
  // The whole reason this is a separate entry point: a settings context or an
  // attachment path named "auto" is a name, not a request for the host name.
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders("auto"), "auto");
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders("auto-lc"), "auto-lc");
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders("auto-uc"), "auto-uc");
  // ...while expand_hostname still resolves them for the submit clients.
  EXPECT_EQ(socket_helpers::expand_hostname("auto"), boost::asio::ip::host_name());
}

TEST(ExpandHostnamePlaceholders, PassesThroughWhatHasNoPlaceholder) {
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders(""), "");
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders("ini:///etc/nsclient/nsclient.ini"), "ini:///etc/nsclient/nsclient.ini");
  // A path token is not ours to resolve - it belongs to the path manager, and
  // has to survive this pass untouched.
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders("${shared-path}/nsclient.ini"), "${shared-path}/nsclient.ini");
}

TEST(ExpandHostnamePlaceholders, MixedWithAPathToken) {
  // The #458 case: the two kinds of placeholder share a syntax and a string,
  // and each pass must leave the other's tokens for it.
  const std::string out = socket_helpers::expand_hostname_placeholders("${shared-path}/${host}-nsclient.ini");
  const std::string host = str::utils::getToken(boost::asio::ip::host_name(), '.').first;
  EXPECT_EQ(out, "${shared-path}/" + host + "-nsclient.ini");
}

// =============================================================================
// expand_hostname_placeholders_in_path / sanitize_path_component
//
// The variant for values that land on the local file system. The host name is
// not fully under the operator's control (DHCP can set it on some systems), so
// what gets substituted into an attachment target or a settings context must
// not be able to carry a path separator or a dots-only component.
// =============================================================================

TEST(SanitizePathComponent, LeavesALegalHostNameAlone) {
  EXPECT_EQ(socket_helpers::sanitize_path_component("my-host"), "my-host");
  EXPECT_EQ(socket_helpers::sanitize_path_component("MY-HOST.example.com"), "MY-HOST.example.com");
  EXPECT_EQ(socket_helpers::sanitize_path_component("srv_01"), "srv_01");
  EXPECT_EQ(socket_helpers::sanitize_path_component(""), "");
}

TEST(SanitizePathComponent, MapsSeparatorsAndFriendsToUnderscore) {
  EXPECT_EQ(socket_helpers::sanitize_path_component("a/b"), "a_b");
  EXPECT_EQ(socket_helpers::sanitize_path_component("a\\b"), "a_b");
  EXPECT_EQ(socket_helpers::sanitize_path_component("../../etc/cron.d/evil"), ".._.._etc_cron.d_evil");
  EXPECT_EQ(socket_helpers::sanitize_path_component("a:b?c"), "a_b_c");
}

TEST(SanitizePathComponent, ADotsOnlyValueIsNotAPathComponent) {
  EXPECT_EQ(socket_helpers::sanitize_path_component("."), "_");
  EXPECT_EQ(socket_helpers::sanitize_path_component(".."), "_");
  // ...but dots inside a name are just dots.
  EXPECT_EQ(socket_helpers::sanitize_path_component("a..b"), "a..b");
}

TEST(ExpandHostnamePlaceholdersInPath, AgreesWithThePlainVariantForARealHostName) {
  // A real system host name is RFC-952 material, so on any sane machine the
  // two variants answer the same - the sanitizer only bites on a hostile name.
  const std::string spec = "${shared-path}/${host}-nsclient.ini|${hostname}|${domain_uc}";
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders_in_path(spec), socket_helpers::expand_hostname_placeholders(spec));
}

TEST(ExpandHostnamePlaceholdersInPath, LeavesNonHostTokensAndShorthandsAlone) {
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders_in_path("${shared-path}/nsclient.ini"), "${shared-path}/nsclient.ini");
  EXPECT_EQ(socket_helpers::expand_hostname_placeholders_in_path("auto"), "auto");
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

TEST(SslOptsTlsMinVersion, EmptyThrows) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "";
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

TEST(SslOptsTlsMaxVersion, EmptyThrows) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.tls_version = "";
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

// A '+' form must resolve to the generic method: the version-pinned methods
// pin the maximum protocol version too, so returning tlsv12 for "1.2+" meant
// TLS 1.2 ONLY and silently excluded TLS 1.3. The floor lives in
// tls_min_version_parser / apply_tls_min_version instead.
TEST(TlsMethodParser, Tls12WithPlus) { EXPECT_EQ(+socket_helpers::tls_method_parser("tlsv1.2+"), +boost::asio::ssl::context::tls); }

TEST(TlsMethodParser, Tls12WithPlusShort) { EXPECT_EQ(+socket_helpers::tls_method_parser("1.2+"), +boost::asio::ssl::context::tls); }

TEST(TlsMethodParser, Tls13WithPlus) { EXPECT_EQ(+socket_helpers::tls_method_parser("1.3+"), +boost::asio::ssl::context::tls); }

// "any" is documented next to the numeric versions and must parse rather than
// throw: generic method, no pin, no floor.
TEST(TlsMethodParser, Any) { EXPECT_EQ(+socket_helpers::tls_method_parser("any"), +boost::asio::ssl::context::tls); }

TEST(TlsMethodParser, InvalidPlusThrows) { EXPECT_THROW(socket_helpers::tls_method_parser("1.4+"), socket_helpers::socket_exception); }

TEST(TlsMethodParser, Tls11) { EXPECT_EQ(+socket_helpers::tls_method_parser("tls1.1"), +boost::asio::ssl::context::tlsv11); }

TEST(TlsMethodParser, Tls10) { EXPECT_EQ(+socket_helpers::tls_method_parser("tls1.0"), +boost::asio::ssl::context::tlsv1); }

TEST(TlsMethodParser, Sslv3) { EXPECT_EQ(+socket_helpers::tls_method_parser("sslv3"), +boost::asio::ssl::context::sslv23); }

TEST(TlsMethodParser, Ssl3) { EXPECT_EQ(+socket_helpers::tls_method_parser("ssl3"), +boost::asio::ssl::context::sslv23); }

TEST(TlsMethodParser, CaseInsensitive) { EXPECT_EQ(+socket_helpers::tls_method_parser("TLSv1.2"), +boost::asio::ssl::context::tlsv12); }

TEST(TlsMethodParser, InvalidThrows) { EXPECT_THROW(socket_helpers::tls_method_parser("invalid"), socket_helpers::socket_exception); }

TEST(TlsMethodParser, EmptyThrows) { EXPECT_THROW(socket_helpers::tls_method_parser(""), socket_helpers::socket_exception); }

// =============================================================================
// SSL-specific: tls_min_version_parser / apply_tls_min_version
// =============================================================================

TEST(TlsMinVersionParser, Tls12PlusAsksForTls12Floor) { EXPECT_EQ(socket_helpers::tls_min_version_parser("1.2+"), TLS1_2_VERSION); }

TEST(TlsMinVersionParser, Tls13PlusAsksForTls13Floor) { EXPECT_EQ(socket_helpers::tls_min_version_parser("tlsv1.3+"), TLS1_3_VERSION); }

TEST(TlsMinVersionParser, Tls10PlusAsksForTls10Floor) { EXPECT_EQ(socket_helpers::tls_min_version_parser("1.0+"), TLS1_VERSION); }

TEST(TlsMinVersionParser, AnExactVersionCarriesNoFloor) {
  // The pinned method already constrains both ends; a floor would be redundant.
  EXPECT_EQ(socket_helpers::tls_min_version_parser("1.2"), 0);
}

TEST(TlsMinVersionParser, AnyCarriesNoFloor) { EXPECT_EQ(socket_helpers::tls_min_version_parser("any"), 0); }

TEST(TlsMinVersionParser, EmptyCarriesNoFloor) { EXPECT_EQ(socket_helpers::tls_min_version_parser(""), 0); }

TEST(TlsMinVersionParser, InvalidPlusThrows) { EXPECT_THROW(socket_helpers::tls_min_version_parser("1.4+"), socket_helpers::socket_exception); }

TEST(ApplyTlsMinVersion, SetsTheFloorOnAGenericContext) {
  boost::asio::ssl::context ctx(socket_helpers::tls_method_parser("1.2+"));
  socket_helpers::apply_tls_min_version(ctx, "1.2+");
  EXPECT_EQ(SSL_CTX_get_min_proto_version(ctx.native_handle()), TLS1_2_VERSION);
}

TEST(ApplyTlsMinVersion, LeavesAFloorlessVersionAlone) {
  boost::asio::ssl::context ctx(socket_helpers::tls_method_parser("any"));
  const long before = SSL_CTX_get_min_proto_version(ctx.native_handle());
  socket_helpers::apply_tls_min_version(ctx, "any");
  EXPECT_EQ(SSL_CTX_get_min_proto_version(ctx.native_handle()), before);
}

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

// =============================================================================
// extract_peer_subject_dn / format_subject_dn_rfc2253
// =============================================================================
//
// extract_peer_subject_dn pulls the verified Subject DN out of an SSL
// session and is what NRPEServer feeds to the permission system as the
// principal. Wrong output here = wrong policy decision in production
// (either silently allowing or silently denying), so the formatting needs
// real coverage.
//
// We can't stand up a TLS session in a unit test, but we CAN construct
// an X509 in memory, set its Subject to a known value, and exercise the
// format helper directly. The format helper is what extract_peer_subject_dn
// delegates to internally, so testing it covers the interesting path.
// The null-input paths of extract_peer_subject_dn we cover separately.

#include <openssl/x509.h>

namespace {

// Build a fresh X509 with the supplied Subject attributes (an ordered
// list of (NID, value) pairs - order matters because the resulting DN
// is printed in the order the entries were appended). Caller takes
// ownership; pair with X509_free.
//
// We construct an X509 rather than just an X509_NAME because the
// public helper takes the cert; this also exercises the
// X509_get_subject_name path inside format_subject_dn_rfc2253.
X509* make_test_cert(const std::vector<std::pair<int, std::string>>& subject_entries) {
  X509* cert = X509_new();
  if (!cert) return nullptr;
  X509_NAME* name = X509_NAME_new();
  if (!name) {
    X509_free(cert);
    return nullptr;
  }
  for (const auto& entry : subject_entries) {
    X509_NAME_add_entry_by_NID(name, entry.first, MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(entry.second.c_str()),
                               static_cast<int>(entry.second.size()), -1, 0);
  }
  X509_set_subject_name(cert, name);
  X509_NAME_free(name);
  return cert;
}

}  // namespace

TEST(ExtractPeerSubjectDn, NullSslReturnsEmpty) {
  // Defensive: extract_peer_subject_dn is called from ssl_connection
  // right after handshake. If something has gone wrong upstream and a
  // null SSL* reaches us, return empty rather than dereferencing.
  EXPECT_EQ("", socket_helpers::extract_peer_subject_dn(nullptr));
}

TEST(FormatSubjectDnRfc2253, NullCertReturnsEmpty) { EXPECT_EQ("", socket_helpers::format_subject_dn_rfc2253(nullptr)); }

TEST(FormatSubjectDnRfc2253, SingleCnRoundTrips) {
  X509* cert = make_test_cert({{NID_commonName, "icinga-master"}});
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("CN=icinga-master", dn);
}

TEST(FormatSubjectDnRfc2253, MultiAttributeSubjectInOrder) {
  // RFC 2253 prints in REVERSE order of how the attributes were added
  // (most-specific first). OpenSSL appends in the order given, then
  // X509_NAME_print_ex with XN_FLAG_RFC2253 emits them right-to-left:
  // the entry added LAST appears FIRST in the output. We pin this so
  // operators (who copy DNs from the log into policy files) get a
  // predictable shape.
  X509* cert = make_test_cert({
      {NID_countryName, "US"},
      {NID_organizationName, "Acme"},
      {NID_commonName, "icinga-master"},
  });
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("CN=icinga-master,O=Acme,C=US", dn);
}

TEST(FormatSubjectDnRfc2253, ValueWithCommaIsEscaped) {
  // A comma in a CN value MUST be backslash-escaped, otherwise the
  // resulting DN string would be ambiguous (the comma is also the
  // attribute separator). If escaping ever regressed, two distinct
  // certs could format to the same DN string and policies could
  // collide.
  X509* cert = make_test_cert({{NID_commonName, "Smith, John"}});
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("CN=Smith\\, John", dn);
}

TEST(FormatSubjectDnRfc2253, ValueWithPlusIsEscaped) {
  // `+` is the multi-valued-RDN separator in RFC 2253 and must be
  // escaped in a value.
  X509* cert = make_test_cert({{NID_commonName, "a+b"}});
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("CN=a\\+b", dn);
}

TEST(FormatSubjectDnRfc2253, LeadingHashIsEscaped) {
  // RFC 2253: a leading `#` in a value indicates a hex-encoded BER
  // value, so a literal leading `#` must be escaped to disambiguate.
  X509* cert = make_test_cert({{NID_commonName, "#hashtag"}});
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("CN=\\#hashtag", dn);
}

TEST(FormatSubjectDnRfc2253, Utf8ValuePreserved) {
  // We tell OpenSSL the value is MBSTRING_UTF8. RFC 2253 keeps UTF-8
  // bytes intact (no transliteration). Use a fixed byte sequence so
  // the test does not depend on the source file encoding: "Café" =
  // 'C' 'a' 'f' 0xC3 0xA9.
  const std::string cn = std::string("Caf\xc3\xa9");
  X509* cert = make_test_cert({{NID_commonName, cn}});
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("CN=" + cn, dn);
}

TEST(FormatSubjectDnRfc2253, EmptySubjectIsEmpty) {
  // X509 with no Subject entries. The output is the empty DN.
  X509* cert = make_test_cert({});
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("", dn);
}

TEST(FormatSubjectDnRfc2253, OrganizationalUnitIsEmittedAsOu) {
  // Sanity-check the attribute shortname mapping for an attribute
  // operators routinely use in monitoring deployments. If OpenSSL ever
  // changes the shortname this test catches it.
  X509* cert = make_test_cert({
      {NID_organizationalUnitName, "monitoring"},
      {NID_commonName, "agent-01"},
  });
  ASSERT_NE(nullptr, cert);
  const std::string dn = socket_helpers::format_subject_dn_rfc2253(cert);
  X509_free(cert);
  EXPECT_EQ("CN=agent-01,OU=monitoring", dn);
}

// =============================================================================
// extract_peer_subject_cn / format_subject_cn_only
// =============================================================================
//
// CN-only extraction is what NRPEServer actually uses today (the DN path
// is implemented but unused because INI key syntax can't carry `=`).
// These tests cover the formatter side; the SSL extraction path itself
// just delegates after retrieving the X509 from the session, same shape
// as the DN tests.

TEST(ExtractPeerSubjectCn, NullSslReturnsEmpty) { EXPECT_EQ("", socket_helpers::extract_peer_subject_cn(nullptr)); }

TEST(FormatSubjectCnOnly, NullCertReturnsEmpty) { EXPECT_EQ("", socket_helpers::format_subject_cn_only(nullptr)); }

TEST(FormatSubjectCnOnly, SingleCnReturnsCnValue) {
  // Smoke test: a cert with just a CN should produce just the value
  // (no "CN=" prefix, no trailing components).
  X509* cert = make_test_cert({{NID_commonName, "icinga-master"}});
  ASSERT_NE(nullptr, cert);
  const std::string cn = socket_helpers::format_subject_cn_only(cert);
  X509_free(cert);
  EXPECT_EQ("icinga-master", cn);
}

TEST(FormatSubjectCnOnly, MultiAttributeSubjectReturnsCnOnly) {
  // Multi-attribute subject: only the CN value should come back.
  // Operators write `NRPEServer:icinga-master` in policy, not
  // `NRPEServer:icinga-master,O=Acme`.
  X509* cert = make_test_cert({
      {NID_countryName, "US"},
      {NID_organizationName, "Acme"},
      {NID_commonName, "icinga-master"},
  });
  ASSERT_NE(nullptr, cert);
  const std::string cn = socket_helpers::format_subject_cn_only(cert);
  X509_free(cert);
  EXPECT_EQ("icinga-master", cn);
}

TEST(FormatSubjectCnOnly, NoCnReturnsEmpty) {
  // A cert with no CN entry should produce empty - and NRPEServer's
  // gating logic (use_principal = !empty()) then falls back to the
  // bare `NRPEServer` subject, which is the safe default.
  X509* cert = make_test_cert({
      {NID_organizationName, "Acme"},
      {NID_countryName, "US"},
  });
  ASSERT_NE(nullptr, cert);
  const std::string cn = socket_helpers::format_subject_cn_only(cert);
  X509_free(cert);
  EXPECT_EQ("", cn);
}

TEST(FormatSubjectCnOnly, MultipleCnsReturnsTheLastOne) {
  // Pins the "use the most-specific CN when several are present"
  // policy. The X509_NAME_add_entry_by_NID order matters here:
  // we append two CNs and expect the second one back. Reordering
  // this expectation requires changing format_subject_cn_only's
  // documented behaviour - it's a deliberate convention, not an
  // accident.
  X509* cert = make_test_cert({
      {NID_commonName, "outer"},
      {NID_commonName, "inner"},
  });
  ASSERT_NE(nullptr, cert);
  const std::string cn = socket_helpers::format_subject_cn_only(cert);
  X509_free(cert);
  EXPECT_EQ("inner", cn);
}

TEST(FormatSubjectCnOnly, Utf8CnPreserved) {
  // Same UTF-8-safe guarantee as the DN formatter: the byte sequence
  // round-trips unchanged. Fixed byte sequence so the test does not
  // depend on the source file encoding.
  const std::string cn_in = std::string("Caf\xc3\xa9");
  X509* cert = make_test_cert({{NID_commonName, cn_in}});
  ASSERT_NE(nullptr, cert);
  const std::string cn_out = socket_helpers::format_subject_cn_only(cert);
  X509_free(cert);
  EXPECT_EQ(cn_in, cn_out);
}

TEST(FormatSubjectCnOnly, CnWithSpaceAndPunctuationPreservedVerbatim) {
  // Unlike the DN formatter, CN-only does NOT escape RFC 2253 specials:
  // we hand the raw UTF-8 value back. Operators who put a literal
  // comma or `=` in a CN end up with that character in the policy key,
  // which is fine for the policy matcher but worth pinning so a future
  // "let's helpfully sanitise this" refactor gets caught.
  X509* cert = make_test_cert({{NID_commonName, "Smith, John"}});
  ASSERT_NE(nullptr, cert);
  const std::string cn = socket_helpers::format_subject_cn_only(cert);
  X509_free(cert);
  EXPECT_EQ("Smith, John", cn);
}

#endif  // USE_SSL

// =============================================================================
// write_certs — generated private keys must not be readable by anyone else
//
// write_certs used a plain fopen(cert, "wb"), so the file landed at
// 0666 & ~umask - 0644 under a normal systemd unit - holding an *unencrypted*
// PKCS#8 private key. A default NRPE server start generates that file when it
// is missing, so every default install published its TLS key to every local
// account. The CA branch was worse: it wrote the CA *private key* into the
// very ca.pem operators are told to hand out to clients.
// =============================================================================

TEST(WriteCerts, CaKeyPathSitsBesideTheCertificate) {
  EXPECT_EQ(socket_helpers::ca_key_path("/etc/nscp/security/ca.pem"), "/etc/nscp/security/ca-key.pem");
  EXPECT_EQ(socket_helpers::ca_key_path("ca.pem"), "ca-key.pem");
  EXPECT_EQ(socket_helpers::ca_key_path("/tmp/my-ca.crt"), "/tmp/my-ca-key.crt");
}

namespace {
class WriteCertsFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-certs-%%%%-%%%%");
    boost::filesystem::create_directories(dir_);
  }
  void TearDown() override {
    boost::system::error_code ignored;
    boost::filesystem::remove_all(dir_, ignored);
  }
  std::string path_of(const std::string &name) const { return (dir_ / name).string(); }
  static std::string read_file(const std::string &path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }
  boost::filesystem::path dir_;
};
}  // namespace

TEST_F(WriteCertsFixture, CertificateFileHoldsKeyAndCertificate) {
  const std::string cert = path_of("certificate.pem");
  ASSERT_NO_THROW(socket_helpers::write_certs(cert, false));

  const std::string content = read_file(cert);
  EXPECT_NE(content.find("PRIVATE KEY"), std::string::npos);
  EXPECT_NE(content.find("BEGIN CERTIFICATE"), std::string::npos);
}

TEST_F(WriteCertsFixture, CaCertificateDoesNotCarryThePrivateKey) {
  const std::string ca = path_of("ca.pem");
  ASSERT_NO_THROW(socket_helpers::write_certs(ca, true));

  const std::string ca_content = read_file(ca);
  EXPECT_NE(ca_content.find("BEGIN CERTIFICATE"), std::string::npos);
  EXPECT_EQ(ca_content.find("PRIVATE KEY"), std::string::npos) << "the file operators hand to clients must not contain the CA key";

  const std::string key = socket_helpers::ca_key_path(ca);
  ASSERT_TRUE(boost::filesystem::is_regular_file(key));
  EXPECT_NE(read_file(key).find("PRIVATE KEY"), std::string::npos);
}

TEST_F(WriteCertsFixture, OverwritingAnExistingFileStillNarrowsIt) {
  const std::string cert = path_of("certificate.pem");
  {
    std::ofstream out(cert.c_str());
    out << "stale";
  }
#ifndef WIN32
  ASSERT_EQ(::chmod(cert.c_str(), 0666), 0);
#endif
  ASSERT_NO_THROW(socket_helpers::write_certs(cert, false));
#ifndef WIN32
  struct stat st = {};
  ASSERT_EQ(::stat(cert.c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0600) << "an existing certificate file keeps its old mode through O_CREAT";
#endif
}

#ifndef WIN32
TEST_F(WriteCertsFixture, PrivateKeyFilesAreOwnerOnly) {
  const std::string cert = path_of("certificate.pem");
  ASSERT_NO_THROW(socket_helpers::write_certs(cert, false));
  struct stat st = {};
  ASSERT_EQ(::stat(cert.c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0600);

  const std::string ca = path_of("ca.pem");
  ASSERT_NO_THROW(socket_helpers::write_certs(ca, true));
  ASSERT_EQ(::stat(socket_helpers::ca_key_path(ca).c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0600);
}
#endif
