// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// End-to-end tests for check_dns. The system-resolver path is driven with IP
// literals, which resolve to themselves without asking any DNS server; the
// server= path talks UDP to a scripted DNS responder on 127.0.0.1 that answers
// with whatever records (or failures) the test hands it.

#include "check_dns.h"

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "check_dns_internal.hpp"

namespace {

using boost::asio::ip::udp;
namespace dns = check_net::check_dns_internal;

struct outcome {
  PB::Common::ResultCode code;
  std::string message;
};

outcome run(const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_dns");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  check_net::check_dns(request, &response);
  outcome r;
  r.code = response.result();
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!r.message.empty()) r.message += "\n";
    r.message += response.lines(i).message();
  }
  return r;
}

bool contains(const std::string &haystack, const std::string &needle) { return haystack.find(needle) != std::string::npos; }

void put16(std::string &p, std::uint16_t v) {
  p.push_back(static_cast<char>((v >> 8) & 0xff));
  p.push_back(static_cast<char>(v & 0xff));
}

// One answer record: owner is a pointer to the question name (offset 12).
std::string answer(std::uint16_t type, const std::string &rdata) {
  std::string rr;
  put16(rr, 0xc00c);
  put16(rr, type);
  put16(rr, 1);  // IN
  put16(rr, 0);
  put16(rr, 300);  // TTL
  put16(rr, static_cast<std::uint16_t>(rdata.size()));
  return rr + rdata;
}

std::string a_record(unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
  return answer(dns::DNS_A, std::string{static_cast<char>(a), static_cast<char>(b), static_cast<char>(c), static_cast<char>(d)});
}

// The reply to `query`: same id and question, the given rcode and answers.
std::string reply_to(const std::string &query, int rcode, const std::vector<std::string> &answers) {
  // Questions are never compressed, so the name ends at the first zero label.
  const std::size_t name_end = query.find('\0', 12);
  const std::string question = query.substr(12, name_end + 1 + 4 - 12);
  std::string p;
  p += query.substr(0, 2);                                        // id
  put16(p, static_cast<std::uint16_t>(0x8000 | 0x0080 | rcode));  // QR, RA, rcode
  put16(p, 1);
  put16(p, static_cast<std::uint16_t>(answers.size()));
  put16(p, 0);
  put16(p, 0);
  p += question;
  for (const std::string &a : answers) p += a;
  return p;
}

// A UDP listener on 127.0.0.1 answering exactly one query with `respond`
// (which may return "" to stay silent). The query it saw is kept.
class dns_responder {
 public:
  explicit dns_responder(std::function<std::string(const std::string &)> respond)
      : socket_(io_, udp::endpoint(boost::asio::ip::address_v4::loopback(), 0)), respond_(std::move(respond)) {
    port_ = socket_.local_endpoint().port();
    thread_ = std::thread([this] {
      char buf[4096];
      udp::endpoint from;
      boost::system::error_code ec;
      const std::size_t n = socket_.receive_from(boost::asio::buffer(buf), from, 0, ec);
      if (ec || stopping_) return;
      query_.assign(buf, n);
      const std::string reply = respond_(query_);
      if (!reply.empty()) socket_.send_to(boost::asio::buffer(reply), from, 0, ec);
    });
  }
  ~dns_responder() {
    stopping_ = true;
    // Unblock a receive that never got a query.
    boost::asio::io_context io;
    udp::socket poke(io, udp::v4());
    boost::system::error_code ec;
    poke.send_to(boost::asio::buffer("x", 1), udp::endpoint(boost::asio::ip::address_v4::loopback(), port_), 0, ec);
    thread_.join();
  }

  std::string port() const { return std::to_string(port_); }
  // Flags word of the query received (0 when none arrived yet).
  std::uint16_t query_flags() const {
    return query_.size() < 4 ? 0 : static_cast<std::uint16_t>((static_cast<unsigned char>(query_[2]) << 8) | static_cast<unsigned char>(query_[3]));
  }
  std::string query() const { return query_; }

 private:
  boost::asio::io_context io_;
  udp::socket socket_;
  std::function<std::string(const std::string &)> respond_;
  unsigned short port_ = 0;
  std::atomic<bool> stopping_{false};
  std::string query_;
  std::thread thread_;
};

}  // namespace

// ============================================================================
// Arguments
// ============================================================================

TEST(CheckDnsArgs, NeedsAHost) {
  const outcome r = run({"type=A"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("No host specified", r.message);
}

TEST(CheckDnsArgs, RejectsAnUnknownRecordType) {
  const outcome r = run({"host=example.com", "type=WKS"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("Invalid DNS record type: WKS", r.message);
}

TEST(CheckDnsArgs, RejectsAnUnknownAddressFamily) {
  const outcome r = run({"host=example.com", "address-family=ipx"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("Invalid address-family: ipx (expected any, ipv4 or ipv6)", r.message);
}

// ============================================================================
// System resolver (A/AAAA without server=)
// ============================================================================

TEST(CheckDnsSystem, LiteralResolvesToItself) {
  const outcome r = run({"host=127.0.0.1", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "127.0.0.1 -> 127.0.0.1 (1) in")) << r.message;
  EXPECT_TRUE(contains(r.message, "[ok]")) << r.message;
}

TEST(CheckDnsSystem, ExpectedAddressesMustAllBePresent) {
  const outcome ok = run({"lookup=127.0.0.1", "expected=127.0.0.1", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, ok.code) << ok.message;

  const outcome bad = run({"host=127.0.0.1", "expected-address=127.0.0.1", "expected-address=192.0.2.1", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, bad.code) << bad.message;
  EXPECT_TRUE(contains(bad.message, "mismatch")) << bad.message;
}

TEST(CheckDnsSystem, AddressFamilyRestrictsTheAnswer) {
  // An IPv4 literal has no IPv6 address.
  const outcome r = run({"host=127.0.0.1", "address-family=ipv6", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_FALSE(contains(r.message, "ok")) << r.message;

  const outcome v6 = run({"host=::1", "type=AAAA", "address-family=ipv6", "detail-syntax=${addresses} ${result}"});
  EXPECT_EQ(PB::Common::ResultCode::OK, v6.code) << v6.message;
  EXPECT_TRUE(contains(v6.message, "::1 ok")) << v6.message;
}

// ============================================================================
// Explicit server over UDP
// ============================================================================

TEST(CheckDnsServer, ARecordsFromTheServer) {
  dns_responder server([](const std::string &q) { return reply_to(q, 0, {a_record(192, 0, 2, 10), a_record(192, 0, 2, 11)}); });
  const outcome r = run({"host=www.example.com", "server=127.0.0.1", "port=" + server.port(), "expected=192.0.2.11", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "www.example.com -> 192.0.2.10,192.0.2.11 (2)")) << r.message;
  // Recursion is requested by default, and the name is on the wire.
  EXPECT_EQ(0x0100, server.query_flags() & 0x0100);
  EXPECT_TRUE(contains(server.query(), dns::encode_qname("www.example.com"))) << "query did not carry the name";
}

TEST(CheckDnsServer, NoRecursionClearsTheRdBit) {
  dns_responder server([](const std::string &q) { return reply_to(q, 0, {a_record(10, 0, 0, 1)}); });
  const outcome r = run({"host=example.com", "server=127.0.0.1", "port=" + server.port(), "norecursion=true", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_EQ(0, server.query_flags() & 0x0100);
}

TEST(CheckDnsServer, MxAndTxtRecordsAreFormatted) {
  dns_responder mx([](const std::string &q) {
    std::string rdata;
    put16(rdata, 10);
    rdata += dns::encode_qname("mail.example.com");
    return reply_to(q, 0, {answer(dns::DNS_MX, rdata)});
  });
  const outcome m = run({"host=example.com", "type=MX", "server=127.0.0.1", "port=" + mx.port(), "detail-syntax=${type} ${addresses}"});
  EXPECT_EQ(PB::Common::ResultCode::OK, m.code) << m.message;
  EXPECT_TRUE(contains(m.message, "MX 10 mail.example.com")) << m.message;

  dns_responder txt([](const std::string &q) { return reply_to(q, 0, {answer(dns::DNS_TXT, std::string("\x0bv=spf1 -all", 12))}); });
  const outcome t = run({"host=example.com", "type=txt", "server=127.0.0.1", "port=" + txt.port(), "expected=v=spf1 -all", "detail-syntax=${addresses}"});
  EXPECT_EQ(PB::Common::ResultCode::OK, t.code) << t.message;
  EXPECT_TRUE(contains(t.message, "v=spf1 -all")) << t.message;
}

TEST(CheckDnsServer, NxdomainIsNotFound) {
  dns_responder server([](const std::string &q) { return reply_to(q, 3, {}); });
  const outcome r = run({"host=nope.example.com", "server=127.0.0.1", "port=" + server.port(), "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "not_found")) << r.message;
}

TEST(CheckDnsServer, EmptyAnswerIsNotFound) {
  dns_responder server([](const std::string &q) { return reply_to(q, 0, {}); });
  const outcome r = run({"host=example.com", "server=127.0.0.1", "port=" + server.port(), "detail-syntax=${result}"});
  EXPECT_TRUE(contains(r.message, "not_found")) << r.message;
}

TEST(CheckDnsServer, ServerFailureIsAnError) {
  dns_responder server([](const std::string &q) { return reply_to(q, 2, {}); });
  const outcome r = run({"host=example.com", "server=127.0.0.1", "port=" + server.port(), "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "error")) << r.message;
}

TEST(CheckDnsServer, GarbageReplyIsMalformed) {
  dns_responder server([](const std::string &) { return std::string("short"); });
  const outcome r = run({"host=example.com", "server=127.0.0.1", "port=" + server.port(), "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "malformed")) << r.message;
}

TEST(CheckDnsServer, SilentServerTimesOut) {
  dns_responder server([](const std::string &) { return std::string(); });
  const outcome r = run({"host=example.com", "server=127.0.0.1", "port=" + server.port(), "timeout=200", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "timeout")) << r.message;
}

TEST(CheckDnsServer, ServerWithoutAnAddressInTheFamilyIsUnresolved) {
  const outcome r = run({"host=example.com", "server=127.0.0.1", "address-family=ipv6", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "server_unresolved")) << r.message;
}
