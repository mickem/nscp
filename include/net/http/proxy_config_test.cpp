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

#include <net/http/proxy_config.hpp>

// =============================================================================
// parse_proxy_url — empty / no scheme
// =============================================================================

TEST(parse_proxy_url, empty_string_returns_none) {
  const http::proxy_config cfg = http::parse_proxy_url("");
  EXPECT_EQ(cfg.type, http::proxy_type::NONE);
  EXPECT_FALSE(cfg.is_set());
}

TEST(parse_proxy_url, no_scheme_separator_returns_none) {
  const http::proxy_config cfg = http::parse_proxy_url("proxy.example.com:3128");
  EXPECT_EQ(cfg.type, http::proxy_type::NONE);
}

TEST(parse_proxy_url, unknown_scheme_returns_none) {
  const http::proxy_config cfg = http::parse_proxy_url("ftp://proxy.example.com:21");
  EXPECT_EQ(cfg.type, http::proxy_type::NONE);
}

// =============================================================================
// parse_proxy_url — HTTP proxy
// =============================================================================

TEST(parse_proxy_url, http_proxy_type) {
  const http::proxy_config cfg = http::parse_proxy_url("http://proxy.corp:3128");
  EXPECT_EQ(cfg.type, http::proxy_type::HTTP);
  EXPECT_TRUE(cfg.is_set());
}

TEST(parse_proxy_url, http_proxy_host) {
  const http::proxy_config cfg = http::parse_proxy_url("http://proxy.corp:3128");
  EXPECT_EQ(cfg.host, "proxy.corp");
}

TEST(parse_proxy_url, http_proxy_port) {
  const http::proxy_config cfg = http::parse_proxy_url("http://proxy.corp:3128");
  EXPECT_EQ(cfg.port, "3128");
}

TEST(parse_proxy_url, http_proxy_default_port_when_missing) {
  const http::proxy_config cfg = http::parse_proxy_url("http://proxy.corp");
  EXPECT_EQ(cfg.port, "3128");
  EXPECT_EQ(cfg.host, "proxy.corp");
}

TEST(parse_proxy_url, http_proxy_strips_trailing_slash) {
  const http::proxy_config cfg = http::parse_proxy_url("http://proxy.corp:3128/");
  EXPECT_EQ(cfg.host, "proxy.corp");
  EXPECT_EQ(cfg.port, "3128");
}

// =============================================================================
// parse_proxy_url — SOCKS5 proxy
// =============================================================================

TEST(parse_proxy_url, socks5_proxy_type) {
  const http::proxy_config cfg = http::parse_proxy_url("socks5://proxy.corp:1080");
  EXPECT_EQ(cfg.type, http::proxy_type::SOCKS5);
}

TEST(parse_proxy_url, socks5_proxy_host_and_port) {
  const http::proxy_config cfg = http::parse_proxy_url("socks5://proxy.corp:1080");
  EXPECT_EQ(cfg.host, "proxy.corp");
  EXPECT_EQ(cfg.port, "1080");
}

TEST(parse_proxy_url, socks5_default_port_when_missing) {
  const http::proxy_config cfg = http::parse_proxy_url("socks5://proxy.corp");
  EXPECT_EQ(cfg.port, "1080");
}

// =============================================================================
// parse_proxy_url — credentials
// =============================================================================

TEST(parse_proxy_url, credentials_username_and_password) {
  const http::proxy_config cfg = http::parse_proxy_url("http://alice:secret@proxy.corp:3128");
  EXPECT_EQ(cfg.username, "alice");
  EXPECT_EQ(cfg.password, "secret");
  EXPECT_EQ(cfg.host, "proxy.corp");
  EXPECT_EQ(cfg.port, "3128");
}

TEST(parse_proxy_url, credentials_username_only) {
  const http::proxy_config cfg = http::parse_proxy_url("http://alice@proxy.corp:3128");
  EXPECT_EQ(cfg.username, "alice");
  EXPECT_TRUE(cfg.password.empty());
}

TEST(parse_proxy_url, credentials_returns_correct_credentials_string) {
  const http::proxy_config cfg = http::parse_proxy_url("http://alice:secret@proxy.corp:3128");
  EXPECT_EQ(cfg.credentials(), "alice:secret");
}

TEST(parse_proxy_url, no_credentials_returns_empty_credentials_string) {
  const http::proxy_config cfg = http::parse_proxy_url("http://proxy.corp:3128");
  EXPECT_EQ(cfg.credentials(), "");
}

TEST(parse_proxy_url, password_with_at_symbol_handled_via_rfind) {
  // password contains percent-encoded '@'; rfind correctly splits at the
  // last '@' (the credential/host separator), and percent-decoding then
  // restores the literal '@' in the password.
  const http::proxy_config cfg = http::parse_proxy_url("http://user:p%40ss@proxy.corp:3128");
  EXPECT_EQ(cfg.username, "user");
  EXPECT_EQ(cfg.password, "p@ss");
  EXPECT_EQ(cfg.host, "proxy.corp");
}

TEST(parse_proxy_url, percent_encoded_username) {
  // RFC 3986 reserved chars in userinfo must be percent-encoded; we should decode them.
  const http::proxy_config cfg = http::parse_proxy_url("http://us%3Aer:pw@proxy.corp:3128");
  EXPECT_EQ(cfg.username, "us:er");
  EXPECT_EQ(cfg.password, "pw");
}

TEST(parse_proxy_url, percent_encoded_password_lowercase_hex) {
  const http::proxy_config cfg = http::parse_proxy_url("http://u:p%2bw@proxy.corp:3128");
  EXPECT_EQ(cfg.password, "p+w");
}

TEST(parse_proxy_url, malformed_percent_sequence_passes_through) {
  // Truncated/invalid percent sequences are kept verbatim rather than swallowed.
  const http::proxy_config cfg = http::parse_proxy_url("http://u:bad%ZZenc@proxy.corp:3128");
  EXPECT_EQ(cfg.password, "bad%ZZenc");
}

// =============================================================================
// should_bypass
// =============================================================================

TEST(should_bypass, empty_list_does_not_bypass) { EXPECT_FALSE(http::should_bypass("server.corp", {})); }

TEST(should_bypass, wildcard_bypasses_all) { EXPECT_TRUE(http::should_bypass("anything.example.com", {"*"})); }

TEST(should_bypass, exact_match_bypasses) { EXPECT_TRUE(http::should_bypass("internal.corp", {"internal.corp"})); }

TEST(should_bypass, exact_match_is_case_insensitive) {
  // Hostnames are case-insensitive per RFC 1035 §2.3.3 — ensure bypass matches both cases.
  EXPECT_TRUE(http::should_bypass("Internal.Corp", {"internal.corp"}));
  EXPECT_TRUE(http::should_bypass("internal.corp", {"INTERNAL.CORP"}));
  EXPECT_TRUE(http::should_bypass("MiXeD.CaSe", {"mixed.case"}));
}

TEST(should_bypass, dot_suffix_is_case_insensitive) {
  EXPECT_TRUE(http::should_bypass("Server.CORP", {".corp"}));
  EXPECT_TRUE(http::should_bypass("server.corp", {".CORP"}));
}

TEST(should_bypass, no_exact_match_does_not_bypass) { EXPECT_FALSE(http::should_bypass("external.com", {"internal.corp"})); }

TEST(should_bypass, dot_suffix_matches_subdomain) { EXPECT_TRUE(http::should_bypass("server.corp", {".corp"})); }

TEST(should_bypass, dot_suffix_matches_deeper_subdomain) { EXPECT_TRUE(http::should_bypass("a.b.corp", {".corp"})); }

TEST(should_bypass, dot_suffix_matches_domain_itself) {
  // ".corp" should also match "corp" (the domain itself without leading dot)
  EXPECT_TRUE(http::should_bypass("corp", {".corp"}));
}

TEST(should_bypass, dot_suffix_does_not_match_unrelated) { EXPECT_FALSE(http::should_bypass("server.example.com", {".corp"})); }

TEST(should_bypass, multiple_patterns_first_match_wins) { EXPECT_TRUE(http::should_bypass("server.corp", {"other.com", ".corp", "something.else"})); }

TEST(should_bypass, multiple_patterns_no_match) { EXPECT_FALSE(http::should_bypass("external.com", {"internal.corp", ".local", "192.168.1.1"})); }

TEST(should_bypass, empty_pattern_is_skipped) { EXPECT_FALSE(http::should_bypass("server.corp", {"", "other.com"})); }

TEST(should_bypass, ip_address_exact_match) { EXPECT_TRUE(http::should_bypass("192.168.1.100", {"192.168.1.100"})); }

TEST(should_bypass, ip_address_no_match) { EXPECT_FALSE(http::should_bypass("10.0.0.1", {"192.168.1.100"})); }

// =============================================================================
// proxy_config defaults
// =============================================================================

TEST(proxy_config, default_constructed_is_none) {
  const http::proxy_config cfg;
  EXPECT_EQ(cfg.type, http::proxy_type::NONE);
  EXPECT_FALSE(cfg.is_set());
}

TEST(proxy_config, default_constructed_no_proxy_list_empty) {
  const http::proxy_config cfg;
  EXPECT_TRUE(cfg.no_proxy.empty());
}

TEST(proxy_config, is_set_false_for_none) {
  http::proxy_config cfg;
  cfg.type = http::proxy_type::NONE;
  EXPECT_FALSE(cfg.is_set());
}

TEST(proxy_config, is_set_true_for_http) {
  http::proxy_config cfg;
  cfg.type = http::proxy_type::HTTP;
  EXPECT_TRUE(cfg.is_set());
}

TEST(proxy_config, is_set_true_for_socks5) {
  http::proxy_config cfg;
  cfg.type = http::proxy_type::SOCKS5;
  EXPECT_TRUE(cfg.is_set());
}
