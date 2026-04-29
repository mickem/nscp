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

#include <boost/make_shared.hpp>
#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_connections.h"
#include "check_connections_internal.hpp"
#include "check_dns.h"
#include "check_http.h"
#include "check_http_internal.hpp"
#include "check_ntp_internal.hpp"
#include "check_ntp_offset.h"
#include "check_tcp.h"
#include "filter.hpp"

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// ============================================================================
// check_ping (existing) - filter_obj
// ============================================================================

TEST(CheckPing, filter_obj_show_per_host) {
  result_container r;
  r.destination_ = "example.com";
  r.ip_ = "1.2.3.4";
  ping_filter::filter_obj o(r);
  EXPECT_EQ(o.show(), "example.com(1.2.3.4)");
  EXPECT_FALSE(o.is_total());
  EXPECT_EQ(o.get_host(), "example.com");
  EXPECT_EQ(o.get_ip(), "1.2.3.4");
}

TEST(CheckPing, filter_obj_total_show) {
  auto t = ping_filter::filter_obj::get_total();
  ASSERT_TRUE(t);
  EXPECT_TRUE(t->is_total());
  EXPECT_EQ(t->show(), "total");
  EXPECT_EQ(t->get_host(), "total");
  EXPECT_EQ(t->get_ip(), "total");
}

TEST(CheckPing, filter_obj_aggregates_counters) {
  result_container r1;
  r1.num_send_ = 4;
  r1.num_replies_ = 4;
  r1.num_timeouts_ = 0;
  r1.time_ = 12;
  result_container r2;
  r2.num_send_ = 4;
  r2.num_replies_ = 2;
  r2.num_timeouts_ = 2;
  r2.time_ = 50;

  auto a = boost::make_shared<ping_filter::filter_obj>(r1);
  auto b = boost::make_shared<ping_filter::filter_obj>(r2);
  auto total = ping_filter::filter_obj::get_total();
  total->add(a);
  total->add(b);
  EXPECT_EQ(total->get_sent(), 8);
  EXPECT_EQ(total->get_recv(), 6);
  EXPECT_EQ(total->get_timeout(), 2);
  EXPECT_EQ(total->get_time(), 62);
}

// ============================================================================
// check_tcp - filter_obj
// ============================================================================

TEST(CheckTcp, filter_obj_defaults) {
  check_net::check_tcp_filter::filter_obj o;
  EXPECT_EQ(o.get_port(), 0);
  EXPECT_EQ(o.get_time(), 0);
  EXPECT_EQ(o.get_connected(), 0);
}

TEST(CheckTcp, filter_obj_show_and_getters) {
  check_net::check_tcp_filter::filter_obj o;
  o.host = "host.example";
  o.port = 22;
  o.time = 17;
  o.result = "ok";
  o.connected = true;
  EXPECT_EQ(o.show(), "host.example:22 (ok)");
  EXPECT_EQ(o.get_host(), "host.example");
  EXPECT_EQ(o.get_port(), 22);
  EXPECT_EQ(o.get_time(), 17);
  EXPECT_EQ(o.get_result(), "ok");
  EXPECT_EQ(o.get_connected(), 1);
}

// ============================================================================
// check_dns - filter_obj
// ============================================================================

TEST(CheckDns, filter_obj_defaults) {
  check_net::check_dns_filter::filter_obj o;
  EXPECT_EQ(o.get_count(), 0);
  EXPECT_EQ(o.get_time(), 0);
  EXPECT_TRUE(o.get_addresses().empty());
}

TEST(CheckDns, filter_obj_show_and_getters) {
  check_net::check_dns_filter::filter_obj o;
  o.host = "example.com";
  o.addresses = "1.1.1.1,2.2.2.2";
  o.count = 2;
  o.time = 10;
  o.result = "ok";
  EXPECT_EQ(o.show(), "example.com -> 1.1.1.1,2.2.2.2 (ok)");
  EXPECT_EQ(o.get_host(), "example.com");
  EXPECT_EQ(o.get_addresses(), "1.1.1.1,2.2.2.2");
  EXPECT_EQ(o.get_count(), 2);
  EXPECT_EQ(o.get_time(), 10);
  EXPECT_EQ(o.get_result(), "ok");
}

// ============================================================================
// check_http - filter_obj and parse_url
// ============================================================================

TEST(CheckHttp, filter_obj_defaults) {
  check_net::check_http_filter::filter_obj o;
  EXPECT_EQ(o.get_port(), 0);
  EXPECT_EQ(o.get_code(), 0);
  EXPECT_EQ(o.get_time(), 0);
  EXPECT_EQ(o.get_size(), 0);
  EXPECT_TRUE(o.get_url().empty());
}

TEST(CheckHttp, filter_obj_show) {
  check_net::check_http_filter::filter_obj o;
  o.url = "http://example.com/";
  o.status_code = 200;
  o.result = "ok";
  EXPECT_EQ(o.show(), "http://example.com/ (200, ok)");
}

TEST(CheckHttp, parse_url_http_default_port) {
  check_net::check_http_internal::parsed_url u;
  ASSERT_TRUE(check_net::check_http_internal::parse_url("http://example.com/path", u));
  EXPECT_EQ(u.protocol, "http");
  EXPECT_EQ(u.host, "example.com");
  EXPECT_EQ(u.port, "80");
  EXPECT_EQ(u.path, "/path");
}

TEST(CheckHttp, parse_url_https_default_port) {
  check_net::check_http_internal::parsed_url u;
  ASSERT_TRUE(check_net::check_http_internal::parse_url("https://example.com", u));
  EXPECT_EQ(u.protocol, "https");
  EXPECT_EQ(u.host, "example.com");
  EXPECT_EQ(u.port, "443");
  EXPECT_EQ(u.path, "/");
}

TEST(CheckHttp, parse_url_explicit_port) {
  check_net::check_http_internal::parsed_url u;
  ASSERT_TRUE(check_net::check_http_internal::parse_url("http://example.com:8080/api/health", u));
  EXPECT_EQ(u.host, "example.com");
  EXPECT_EQ(u.port, "8080");
  EXPECT_EQ(u.path, "/api/health");
}

TEST(CheckHttp, parse_url_uppercase_protocol_is_lowercased) {
  check_net::check_http_internal::parsed_url u;
  ASSERT_TRUE(check_net::check_http_internal::parse_url("HTTP://Example.com:8080/X", u));
  EXPECT_EQ(u.protocol, "http");
}

TEST(CheckHttp, parse_url_rejects_bad_input) {
  check_net::check_http_internal::parsed_url u;
  EXPECT_FALSE(check_net::check_http_internal::parse_url("not a url", u));
  EXPECT_FALSE(check_net::check_http_internal::parse_url("ftp://example.com/", u));
  EXPECT_FALSE(check_net::check_http_internal::parse_url("http:///nohost", u));
}

// ============================================================================
// check_ntp_offset - filter_obj and ntp helpers
// ============================================================================

TEST(CheckNtp, filter_obj_defaults) {
  check_net::check_ntp_filter::filter_obj o;
  EXPECT_EQ(o.get_offset(), 0);
  EXPECT_EQ(o.get_offset_signed(), 0);
  EXPECT_EQ(o.get_stratum(), 0);
  EXPECT_EQ(o.get_time(), 0);
}

TEST(CheckNtp, filter_obj_offset_is_absolute_value) {
  check_net::check_ntp_filter::filter_obj o;
  o.offset_ms = -500;
  EXPECT_EQ(o.get_offset(), 500);
  EXPECT_EQ(o.get_offset_signed(), -500);
  o.offset_ms = 1234;
  EXPECT_EQ(o.get_offset(), 1234);
  EXPECT_EQ(o.get_offset_signed(), 1234);
}

TEST(CheckNtp, ntp_to_unix_ms_zero_is_sentinel) {
  EXPECT_EQ(check_net::check_ntp_internal::ntp_to_unix_ms(0, 0), 0);
}

TEST(CheckNtp, ntp_to_unix_ms_unix_epoch) {
  // The unix epoch (1970-01-01 00:00:00 UTC) is exactly kNtpUnixDelta seconds
  // after the NTP epoch.
  const std::uint32_t secs = static_cast<std::uint32_t>(check_net::check_ntp_internal::kNtpUnixDelta);
  EXPECT_EQ(check_net::check_ntp_internal::ntp_to_unix_ms(secs, 0), 0);
}

TEST(CheckNtp, ntp_to_unix_ms_one_second_after_unix_epoch) {
  const std::uint32_t secs = static_cast<std::uint32_t>(check_net::check_ntp_internal::kNtpUnixDelta + 1);
  EXPECT_EQ(check_net::check_ntp_internal::ntp_to_unix_ms(secs, 0), 1000);
}

TEST(CheckNtp, ntp_to_unix_ms_fraction_half_second) {
  // frac = 2^31 represents 0.5 seconds.
  const std::uint32_t secs = static_cast<std::uint32_t>(check_net::check_ntp_internal::kNtpUnixDelta);
  const std::uint32_t half = 0x80000000u;
  EXPECT_EQ(check_net::check_ntp_internal::ntp_to_unix_ms(secs, half), 500);
}

TEST(CheckNtp, ntp_to_unix_ms_pre_unix_epoch_returns_zero) {
  // A timestamp from the NTP epoch year 1900 (well before the unix epoch)
  // should not underflow into a huge positive value.
  EXPECT_EQ(check_net::check_ntp_internal::ntp_to_unix_ms(1, 0), 0);
}

TEST(CheckNtp, ntp_offset_ms_zero_when_balanced) {
  // Symmetric path: T2-T1 == T4-T3, so offset is zero.
  EXPECT_EQ(check_net::check_ntp_internal::ntp_offset_ms(1000, 1010, 1011, 1021), 0);
}

TEST(CheckNtp, ntp_offset_ms_positive_when_server_ahead) {
  // Server clock is 100ms ahead of local: T2 = T1 + delay/2 + 100, T3 = T2, T4 = T3 + delay/2 - 100.
  EXPECT_EQ(check_net::check_ntp_internal::ntp_offset_ms(0, 105, 106, 11), 100);
}

// ============================================================================
// check_connections - filter_obj and linux_tcp_state
// ============================================================================

TEST(CheckConnections, filter_obj_defaults) {
  check_net::check_connections_filter::filter_obj o;
  EXPECT_EQ(o.get_count(), 0);
  EXPECT_EQ(o.get_total(), 0);
  EXPECT_EQ(o.get_established(), 0);
  EXPECT_EQ(o.get_listen(), 0);
  EXPECT_EQ(o.get_udp(), 0);
}

TEST(CheckConnections, filter_obj_show) {
  check_net::check_connections_filter::filter_obj o;
  o.protocol = "tcp";
  o.state = "ESTABLISHED";
  o.count = 7;
  EXPECT_EQ(o.show(), "tcp/ESTABLISHED=7");
}

TEST(CheckConnections, linux_tcp_state_known_values) {
  using check_net::check_connections_internal::linux_tcp_state;
  EXPECT_STREQ(linux_tcp_state(0x01), "ESTABLISHED");
  EXPECT_STREQ(linux_tcp_state(0x02), "SYN_SENT");
  EXPECT_STREQ(linux_tcp_state(0x03), "SYN_RECV");
  EXPECT_STREQ(linux_tcp_state(0x04), "FIN_WAIT1");
  EXPECT_STREQ(linux_tcp_state(0x05), "FIN_WAIT2");
  EXPECT_STREQ(linux_tcp_state(0x06), "TIME_WAIT");
  EXPECT_STREQ(linux_tcp_state(0x07), "CLOSE");
  EXPECT_STREQ(linux_tcp_state(0x08), "CLOSE_WAIT");
  EXPECT_STREQ(linux_tcp_state(0x09), "LAST_ACK");
  EXPECT_STREQ(linux_tcp_state(0x0A), "LISTEN");
  EXPECT_STREQ(linux_tcp_state(0x0B), "CLOSING");
}

TEST(CheckConnections, linux_tcp_state_unknown_value) {
  EXPECT_STREQ(check_net::check_connections_internal::linux_tcp_state(0x42), "UNKNOWN");
  EXPECT_STREQ(check_net::check_connections_internal::linux_tcp_state(0), "UNKNOWN");
}
