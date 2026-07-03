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

#include <gtest/gtest.h>

#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_connections.h"
#include "check_connections_internal.hpp"
#include "check_dns.h"
#include "check_dns_internal.hpp"
#include "check_http.h"
#include "check_http_internal.hpp"
#include "check_nsclient_web_online.h"
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

  auto a = std::make_shared<ping_filter::filter_obj>(r1);
  auto b = std::make_shared<ping_filter::filter_obj>(r2);
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
// check_dns - DNS wire protocol (build_query / parse_response)
// ============================================================================

namespace {
// Assemble a packet from raw bytes.
std::string bytes(std::initializer_list<int> b) {
  std::string s;
  for (int v : b) s.push_back(static_cast<char>(v & 0xff));
  return s;
}
}  // namespace

TEST(CheckDnsWire, TypeNameMapping) {
  using namespace check_net::check_dns_internal;
  EXPECT_EQ(type_from_string("a"), DNS_A);
  EXPECT_EQ(type_from_string("AAAA"), DNS_AAAA);
  EXPECT_EQ(type_from_string("mx"), DNS_MX);
  EXPECT_EQ(type_from_string("TXT"), DNS_TXT);
  EXPECT_EQ(type_from_string("bogus"), -1);
  EXPECT_EQ(type_to_string(DNS_MX), "MX");
}

TEST(CheckDnsWire, BuildQueryEncodesQuestionAndEdns) {
  using namespace check_net::check_dns_internal;
  const std::string q = build_query(0x1234, "a.com", DNS_A, true);
  // Header: id, RD flag, qdcount=1, arcount=1 (the EDNS0 OPT record).
  EXPECT_EQ(static_cast<unsigned char>(q[0]), 0x12);
  EXPECT_EQ(static_cast<unsigned char>(q[1]), 0x34);
  EXPECT_EQ(static_cast<unsigned char>(q[2]), 0x01);  // RD
  EXPECT_EQ(static_cast<unsigned char>(q[5]), 0x01);  // qdcount low byte
  EXPECT_EQ(static_cast<unsigned char>(q[11]), 0x01);  // arcount low byte
  // Question name: 1 'a' 3 'c' 'o' 'm' 0
  EXPECT_EQ(q.substr(12, 7), std::string("\x01" "a" "\x03" "com" "\x00", 7));
}

TEST(CheckDnsWire, BuildQueryNoRecursion) {
  using namespace check_net::check_dns_internal;
  const std::string q = build_query(1, "x", DNS_A, false);
  EXPECT_EQ(static_cast<unsigned char>(q[2]), 0x00);  // RD cleared
}

TEST(CheckDnsWire, ParsesARecordWithCompression) {
  using namespace check_net::check_dns_internal;
  const std::string pkt = bytes({
      0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,  // header
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00,       // qname
      0x00, 0x01, 0x00, 0x01,                                                    // qtype A, qclass IN
      0xc0, 0x0c,                                                                // answer name -> ptr to offset 12
      0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04,               // type A, class IN, ttl 60, rdlen 4
      0x5d, 0xb8, 0xd8, 0x22,                                                    // 93.184.216.34
  });
  const dns_result r = parse_response(pkt, DNS_A);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.rcode, 0);
  ASSERT_EQ(r.records.size(), 1u);
  EXPECT_EQ(r.records[0], "93.184.216.34");
}

TEST(CheckDnsWire, ParsesMxRecord) {
  using namespace check_net::check_dns_internal;
  const std::string pkt = bytes({
      0x00, 0x01, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x01, 'a', 0x00, 0x00, 0x0f, 0x00, 0x01,        // qname "a", qtype MX(15), qclass IN
      0xc0, 0x0c, 0x00, 0x0f, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c,
      0x00, 0x06, 0x00, 0x0a, 0x03, 'm', 'x', '1', 0x00,  // rdlen 6: pref 10 + "mx1"
  });
  const dns_result r = parse_response(pkt, DNS_MX);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ(r.records.size(), 1u);
  EXPECT_EQ(r.records[0], "10 mx1");
}

TEST(CheckDnsWire, ParsesTxtRecord) {
  using namespace check_net::check_dns_internal;
  const std::string pkt = bytes({
      0x00, 0x01, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x01, 'a', 0x00, 0x00, 0x10, 0x00, 0x01,        // qname "a", qtype TXT(16)
      0xc0, 0x0c, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c,
      0x00, 0x06, 0x05, 'h', 'e', 'l', 'l', 'o',      // rdlen 6: one 5-char string "hello"
  });
  const dns_result r = parse_response(pkt, DNS_TXT);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ(r.records.size(), 1u);
  EXPECT_EQ(r.records[0], "hello");
}

TEST(CheckDnsWire, NxdomainRcode) {
  using namespace check_net::check_dns_internal;
  const std::string pkt = bytes({
      0x00, 0x01, 0x81, 0x83, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // rcode 3, ancount 0
      0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01,
  });
  const dns_result r = parse_response(pkt, DNS_A);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.rcode, 3);
  EXPECT_TRUE(r.records.empty());
}

TEST(CheckDnsWire, TruncatedPacketIsNotOk) {
  using namespace check_net::check_dns_internal;
  EXPECT_FALSE(parse_response(std::string("\x00\x01", 2), DNS_A).ok);
}

// ============================================================================
// check_tcp - service presets and response keyword
// ============================================================================

TEST(CheckTcp, ResponseGetter) {
  check_net::check_tcp_filter::filter_obj o;
  o.response = "SSH-2.0-OpenSSH";
  EXPECT_EQ(o.get_response(), "SSH-2.0-OpenSSH");
}

TEST(CheckTcp, ServicePresetLookup) {
  const auto *ssh = check_net::find_service_preset("ssh");
  ASSERT_NE(ssh, nullptr);
  EXPECT_EQ(ssh->port, 22);
  EXPECT_STREQ(ssh->expect_regex, "^SSH-");
  const auto *smtp = check_net::find_service_preset("SMTP");
  ASSERT_NE(smtp, nullptr);
  EXPECT_EQ(smtp->port, 25);
  EXPECT_EQ(check_net::find_service_preset("bogus"), nullptr);
}

// ============================================================================
// check_nsclient_web_online - REST result JSON parsing
// ============================================================================

TEST(CheckNsclientWebOnline, ParsesResultAndMessage) {
  int code = -1;
  std::string msg;
  ASSERT_TRUE(check_net::parse_nsclient_web_online_result(
      R"({"command":"check_cpu","result":2,"lines":[{"message":"CPU load is high","perf":{}}]})", code, msg));
  EXPECT_EQ(code, 2);
  EXPECT_EQ(msg, "CPU load is high");
}

TEST(CheckNsclientWebOnline, JoinsMultipleLines) {
  int code = -1;
  std::string msg;
  ASSERT_TRUE(check_net::parse_nsclient_web_online_result(R"({"result":0,"lines":[{"message":"a"},{"message":"b"}]})", code, msg));
  EXPECT_EQ(code, 0);
  EXPECT_EQ(msg, "a\nb");
}

TEST(CheckNsclientWebOnline, MissingResultFails) {
  int code = -1;
  std::string msg;
  EXPECT_FALSE(check_net::parse_nsclient_web_online_result(R"({"lines":[]})", code, msg));
}

TEST(CheckNsclientWebOnline, InvalidJsonFails) {
  int code = -1;
  std::string msg;
  EXPECT_FALSE(check_net::parse_nsclient_web_online_result("not json", code, msg));
}

TEST(CheckNsclientWebOnline, MissingLinesIsOkWithEmptyMessage) {
  int code = -1;
  std::string msg = "x";
  ASSERT_TRUE(check_net::parse_nsclient_web_online_result(R"({"result":3})", code, msg));
  EXPECT_EQ(code, 3);
  EXPECT_TRUE(msg.empty());
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

TEST(CheckNtp, ntp_to_unix_ms_zero_is_sentinel) { EXPECT_EQ(check_net::check_ntp_internal::ntp_to_unix_ms(0, 0), 0); }

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
