// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <memory>
#include <net/address_family.hpp>
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

TEST(CheckTcp, filter_obj_certificate_defaults_to_absent) {
  // A plain (or failed) connection has no certificate. -1 is the "unknown"
  // day count, and has_certificate is what distinguishes it from a real
  // negative value.
  check_net::check_tcp_filter::filter_obj o;
  EXPECT_EQ(o.get_has_certificate(), 0);
  EXPECT_EQ(o.get_ssl_expiry_days(), -1);
}

TEST(CheckTcp, filter_obj_certificate_getters) {
  check_net::check_tcp_filter::filter_obj o;
  o.has_certificate = true;
  o.ssl_expiry_days = 42;
  EXPECT_EQ(o.get_has_certificate(), 1);
  EXPECT_EQ(o.get_ssl_expiry_days(), 42);
}

TEST(CheckTcp, filter_obj_expired_certificate_is_negative_but_present) {
  // The case has_certificate exists for: an expired certificate reports a
  // negative day count, which must not read as "there was no certificate".
  check_net::check_tcp_filter::filter_obj o;
  o.has_certificate = true;
  o.ssl_expiry_days = -7;
  EXPECT_EQ(o.get_has_certificate(), 1);
  EXPECT_EQ(o.get_ssl_expiry_days(), -7);
}

TEST(CheckSsh, filter_obj_inherits_the_certificate_fields_unset) {
  // check_ssh shares the TCP object but never negotiates TLS, so the fields
  // exist and stay at their "no certificate" defaults.
  check_net::check_ssh_filter::filter_obj o;
  o.response = "SSH-2.0-OpenSSH_9.6p1";
  o.post_read();
  EXPECT_EQ(o.get_has_certificate(), 0);
  EXPECT_EQ(o.get_ssl_expiry_days(), -1);
  EXPECT_EQ(o.get_software(), "OpenSSH");
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
  EXPECT_FALSE(ssh->tls);
  const auto *smtp = check_net::find_service_preset("SMTP");
  ASSERT_NE(smtp, nullptr);
  EXPECT_EQ(smtp->port, 25);
  EXPECT_FALSE(smtp->tls);
  EXPECT_EQ(check_net::find_service_preset("bogus"), nullptr);
}

TEST(CheckTcp, TlsServicePresets) {
  // The s-prefixed presets use implicit TLS on their well-known secure ports.
  const auto *spop = check_net::find_service_preset("spop");
  ASSERT_NE(spop, nullptr);
  EXPECT_EQ(spop->port, 995);
  EXPECT_TRUE(spop->tls);
  EXPECT_STREQ(spop->expect_regex, "^\\+OK");

  const auto *simap = check_net::find_service_preset("SIMAP");
  ASSERT_NE(simap, nullptr);
  EXPECT_EQ(simap->port, 993);
  EXPECT_TRUE(simap->tls);

  const auto *ssmtp = check_net::find_service_preset("ssmtp");
  ASSERT_NE(ssmtp, nullptr);
  EXPECT_EQ(ssmtp->port, 465);
  EXPECT_TRUE(ssmtp->tls);
  EXPECT_STREQ(ssmtp->expect_regex, "^220");
}

// ============================================================================
// address-family selection (shared by every CheckNet check)
// ============================================================================

TEST(AddressFamily, ParsesTheCanonicalNames) {
  net::address_family af = net::address_family::ipv6;
  ASSERT_TRUE(net::parse_address_family("any", af));
  EXPECT_EQ(net::address_family::any, af);
  ASSERT_TRUE(net::parse_address_family("ipv4", af));
  EXPECT_EQ(net::address_family::ipv4, af);
  ASSERT_TRUE(net::parse_address_family("ipv6", af));
  EXPECT_EQ(net::address_family::ipv6, af);
}

TEST(AddressFamily, ParsesAliasesAndIsCaseInsensitive) {
  net::address_family af = net::address_family::any;
  for (const char *v : {"4", "v4", "inet", "IPv4", "IPV4", "V4"}) {
    ASSERT_TRUE(net::parse_address_family(v, af)) << v;
    EXPECT_EQ(net::address_family::ipv4, af) << v;
  }
  for (const char *v : {"6", "v6", "inet6", "IPv6", "INET6"}) {
    ASSERT_TRUE(net::parse_address_family(v, af)) << v;
    EXPECT_EQ(net::address_family::ipv6, af) << v;
  }
  for (const char *v : {"any", "ANY", "both", "unspec"}) {
    ASSERT_TRUE(net::parse_address_family(v, af)) << v;
    EXPECT_EQ(net::address_family::any, af) << v;
  }
}

TEST(AddressFamily, EmptyMeansAnySoAnOmittedArgumentKeepsTheOldBehaviour) {
  net::address_family af = net::address_family::ipv6;
  ASSERT_TRUE(net::parse_address_family("", af));
  EXPECT_EQ(net::address_family::any, af);
}

TEST(AddressFamily, UnknownValueIsRejectedAndLeavesTheTargetUntouched) {
  // Rejecting rather than falling back to `any` is the point: a typo like
  // "ipv64" must be reported, not silently ignored, or the check would quietly
  // stop testing the family the user asked for.
  net::address_family af = net::address_family::ipv4;
  EXPECT_FALSE(net::parse_address_family("ipv64", af));
  EXPECT_FALSE(net::parse_address_family("v5", af));
  EXPECT_FALSE(net::parse_address_family("yes", af));
  EXPECT_FALSE(net::parse_address_family("46", af));
  EXPECT_EQ(net::address_family::ipv4, af);
}

TEST(AddressFamily, ToStringRoundTrips) {
  for (const auto af : {net::address_family::any, net::address_family::ipv4, net::address_family::ipv6}) {
    net::address_family parsed = net::address_family::any;
    ASSERT_TRUE(net::parse_address_family(net::to_string(af), parsed));
    EXPECT_EQ(af, parsed);
  }
}

// ============================================================================
// check_ssh - SSH identification string (RFC 4253 4.2) parsing
// ============================================================================

namespace {
check_net::check_ssh_internal::ssh_banner parse_banner(const std::string &raw) {
  check_net::check_ssh_internal::ssh_banner b;
  EXPECT_TRUE(check_net::check_ssh_internal::parse_ssh_banner(raw, b)) << "failed to parse: " << raw;
  return b;
}
}  // namespace

TEST(CheckSsh, ParsesOpenSshBannerWithComments) {
  const auto b = parse_banner("SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5\r\n");
  EXPECT_EQ("SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5", b.banner);
  EXPECT_EQ("2.0", b.protocol);
  EXPECT_EQ(2, b.protocol_major);
  EXPECT_EQ(0, b.protocol_minor);
  EXPECT_EQ("OpenSSH_9.6p1", b.version);
  EXPECT_EQ("OpenSSH", b.software);
  EXPECT_EQ("9.6p1", b.software_version);
  EXPECT_EQ("Ubuntu-3ubuntu13.5", b.comments);
}

TEST(CheckSsh, ParsesBannerWithoutComments) {
  const auto b = parse_banner("SSH-2.0-dropbear_2022.83\r\n");
  EXPECT_EQ("dropbear", b.software);
  EXPECT_EQ("2022.83", b.software_version);
  EXPECT_EQ("", b.comments);
}

TEST(CheckSsh, ProtocolOneNineNineIsMajorOne) {
  // "1.99" advertises 2.0 with 1.x backwards compatibility, i.e. the server
  // still speaks the insecure SSHv1 — protocol_major < 2 must catch it.
  const auto b = parse_banner("SSH-1.99-OpenSSH_3.9p1\r\n");
  EXPECT_EQ("1.99", b.protocol);
  EXPECT_EQ(1, b.protocol_major);
  EXPECT_EQ(99, b.protocol_minor);
}

TEST(CheckSsh, ProtocolOneFiveIsSshV1Only) {
  const auto b = parse_banner("SSH-1.5-OpenSSH_2.9\r\n");
  EXPECT_EQ(1, b.protocol_major);
  EXPECT_EQ(5, b.protocol_minor);
}

TEST(CheckSsh, SoftwareSplitsOnTheLastUnderscoreBeforeADigit) {
  // A first-underscore split would cut these in the wrong place.
  const auto win = parse_banner("SSH-2.0-OpenSSH_for_Windows_9.5\r\n");
  EXPECT_EQ("OpenSSH_for_Windows", win.software);
  EXPECT_EQ("9.5", win.software_version);

  const auto sun = parse_banner("SSH-2.0-Sun_SSH_1.1\r\n");
  EXPECT_EQ("Sun_SSH", sun.software);
  EXPECT_EQ("1.1", sun.software_version);
}

TEST(CheckSsh, SoftwareWithoutAVersionKeepsTheWholeName) {
  // Some servers publish an opaque build id rather than a version.
  const auto b = parse_banner("SSH-2.0-GitLab-SSHD\r\n");
  EXPECT_EQ("GitLab-SSHD", b.software);
  EXPECT_EQ("", b.software_version);
  // The full version string is still there for regex matching.
  EXPECT_EQ("GitLab-SSHD", b.version);
}

TEST(CheckSsh, UnderscoreNotFollowedByADigitIsNotAVersionSeparator) {
  const auto b = parse_banner("SSH-2.0-Foo_Bar\r\n");
  EXPECT_EQ("Foo_Bar", b.software);
  EXPECT_EQ("", b.software_version);
}

TEST(CheckSsh, SkipsPreambleLinesBeforeTheIdentificationString) {
  // RFC 4253 lets a server send other lines before the identification string.
  const auto b = parse_banner("You are being watched\r\nAuthorized use only\r\nSSH-2.0-OpenSSH_8.9p1\r\n");
  EXPECT_EQ("SSH-2.0-OpenSSH_8.9p1", b.banner);
  EXPECT_EQ("OpenSSH", b.software);
  EXPECT_EQ("8.9p1", b.software_version);
}

TEST(CheckSsh, HandlesBareNewlineAndNoTrailingNewline) {
  const auto lf = parse_banner("SSH-2.0-OpenSSH_9.6p1\n");
  EXPECT_EQ("OpenSSH", lf.software);
  const auto none = parse_banner("SSH-2.0-OpenSSH_9.6p1");
  EXPECT_EQ("OpenSSH", none.software);
  EXPECT_EQ("9.6p1", none.software_version);
}

TEST(CheckSsh, NonSshAndMalformedInputIsRejected) {
  check_net::check_ssh_internal::ssh_banner b;
  EXPECT_FALSE(check_net::check_ssh_internal::parse_ssh_banner("", b));
  EXPECT_FALSE(check_net::check_ssh_internal::parse_ssh_banner("HELLO not ssh\r\n", b));
  EXPECT_FALSE(check_net::check_ssh_internal::parse_ssh_banner("220 mail.example.com ESMTP\r\n", b));
  // "SSH-" prefixed but not a usable identification string.
  EXPECT_FALSE(check_net::check_ssh_internal::parse_ssh_banner("SSH-2.0\r\n", b));    // no software part
  EXPECT_FALSE(check_net::check_ssh_internal::parse_ssh_banner("SSH-2.0-\r\n", b));   // empty software part
  EXPECT_FALSE(check_net::check_ssh_internal::parse_ssh_banner("SSH--OpenSSH\r\n", b));  // empty protocol
  // A rejected parse must leave the output untouched rather than half-filled.
  EXPECT_EQ("", b.banner);
  EXPECT_EQ("", b.protocol);
  EXPECT_EQ(0, b.protocol_major);
}

TEST(CheckSsh, NonNumericProtocolLeavesTheNumbersAtZero) {
  const auto b = parse_banner("SSH-x.y-OpenSSH_9.6p1\r\n");
  EXPECT_EQ("x.y", b.protocol);
  EXPECT_EQ(0, b.protocol_major);
  EXPECT_EQ(0, b.protocol_minor);
}

TEST(CheckSsh, FilterObjExposesTheParsedBannerAndKeepsTcpFields) {
  check_net::check_ssh_filter::filter_obj o;
  o.host = "srv";
  o.port = 22;
  o.result = "ok";
  o.response = "SSH-2.0-OpenSSH_9.6p1 Debian-2";
  o.post_read();

  // TCP fields still behave as they do for check_tcp.
  EXPECT_EQ("srv", o.get_host());
  EXPECT_EQ(22, o.get_port());
  EXPECT_EQ("srv:22 (ok)", o.show());
  // ...plus the parsed identification string.
  EXPECT_EQ("SSH-2.0-OpenSSH_9.6p1 Debian-2", o.get_banner());
  EXPECT_EQ("2.0", o.get_protocol());
  EXPECT_EQ(2, o.get_protocol_major());
  EXPECT_EQ(0, o.get_protocol_minor());
  EXPECT_EQ("OpenSSH_9.6p1", o.get_version());
  EXPECT_EQ("OpenSSH", o.get_software());
  EXPECT_EQ("9.6p1", o.get_software_version());
  EXPECT_EQ("Debian-2", o.get_comments());
}

TEST(CheckSsh, FilterObjFieldsStayEmptyWhenNoBannerWasRead) {
  // A refused/timed-out connection reads nothing; the banner keywords must be
  // empty (and the numbers 0) rather than carrying stale or invented data.
  check_net::check_ssh_filter::filter_obj o;
  o.host = "srv";
  o.port = 22;
  o.result = "refused";
  o.post_read();

  EXPECT_EQ("", o.get_banner());
  EXPECT_EQ("", o.get_protocol());
  EXPECT_EQ("", o.get_version());
  EXPECT_EQ("", o.get_software());
  EXPECT_EQ(0, o.get_protocol_major());
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

TEST(CheckHttp, parse_url_ipv6_literal) {
  // An IPv6 literal is bracketed exactly because it is full of colons; a naive
  // find(':') would take "::1" apart at the first one and leave a port of ":1".
  check_net::check_http_internal::parsed_url u;
  ASSERT_TRUE(check_net::check_http_internal::parse_url("http://[::1]:8080/api", u));
  EXPECT_EQ(u.host, "::1");
  EXPECT_EQ(u.port, "8080");
  EXPECT_EQ(u.path, "/api");

  ASSERT_TRUE(check_net::check_http_internal::parse_url("http://[2001:db8::1]/", u));
  EXPECT_EQ(u.host, "2001:db8::1");
  EXPECT_EQ(u.port, "80");  // default applies to a bracketed host too
  EXPECT_EQ(u.path, "/");

  ASSERT_TRUE(check_net::check_http_internal::parse_url("https://[2001:db8::1]", u));
  EXPECT_EQ(u.host, "2001:db8::1");
  EXPECT_EQ(u.port, "443");
  EXPECT_EQ(u.path, "/");
}

TEST(CheckHttp, parse_url_malformed_ipv6_literal_is_rejected) {
  check_net::check_http_internal::parsed_url u;
  EXPECT_FALSE(check_net::check_http_internal::parse_url("http://[::1/", u));      // unterminated bracket
  EXPECT_FALSE(check_net::check_http_internal::parse_url("http://[]/", u));        // empty host
  EXPECT_FALSE(check_net::check_http_internal::parse_url("http://[::1]x8080/", u));  // junk after the bracket
  EXPECT_FALSE(check_net::check_http_internal::parse_url("http://[::1]:/", u));    // empty port
}

TEST(CheckHttp, host_header_value_brackets_only_ipv6) {
  // RFC 7230 wants the brackets back in the Host header, but a name or an IPv4
  // literal must be sent unchanged.
  EXPECT_EQ("[::1]", check_net::check_http_internal::host_header_value("::1"));
  EXPECT_EQ("[2001:db8::1]", check_net::check_http_internal::host_header_value("2001:db8::1"));
  EXPECT_EQ("example.com", check_net::check_http_internal::host_header_value("example.com"));
  EXPECT_EQ("192.0.2.1", check_net::check_http_internal::host_header_value("192.0.2.1"));
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

TEST(CheckHttp, resolve_redirect_absolute) {
  using check_net::check_http_internal::resolve_redirect;
  EXPECT_EQ(resolve_redirect("http://a.com/x", "https://b.com/y"), "https://b.com/y");
}

TEST(CheckHttp, resolve_redirect_root_relative) {
  using check_net::check_http_internal::resolve_redirect;
  EXPECT_EQ(resolve_redirect("http://a.com:8080/dir/page", "/login"), "http://a.com:8080/login");
}

TEST(CheckHttp, resolve_redirect_protocol_relative) {
  using check_net::check_http_internal::resolve_redirect;
  EXPECT_EQ(resolve_redirect("https://a.com/x", "//cdn.b.com/z"), "https://cdn.b.com/z");
}

TEST(CheckHttp, resolve_redirect_path_relative) {
  using check_net::check_http_internal::resolve_redirect;
  // Resolves against the directory of the current path.
  EXPECT_EQ(resolve_redirect("http://a.com/dir/page", "next"), "http://a.com:80/dir/next");
  EXPECT_EQ(resolve_redirect("https://a.com/", "home"), "https://a.com:443/home");
}

TEST(CheckHttp, ssl_expiry_days_default_is_minus_one) {
  check_net::check_http_filter::filter_obj o;
  EXPECT_EQ(o.get_ssl_expiry_days(), -1);
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
