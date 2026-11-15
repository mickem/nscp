// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// End-to-end tests for check_tcp / check_ssh: the real check entry points,
// argument parsing included, driven against a scripted server on the loopback
// interface. Every conversation the check can have - connect, send/expect,
// service presets, STARTTLS negotiation and a real TLS handshake against a
// certificate minted in-process - is played out on 127.0.0.1, so nothing here
// depends on the network the test happens to run on.

#include "check_tcp.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#include "loopback_test_support.hpp"

namespace {

using namespace check_net_test;

struct result {
  PB::Common::ResultCode code;
  std::string message;
  std::string perf;
};

result run(void (*check)(const std::string &, const PB::Commands::QueryRequestMessage::Request &, PB::Commands::QueryResponseMessage::Response *),
           const std::vector<std::string> &args, const std::string &default_ca = "none") {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_tcp");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  check(default_ca, request, &response);
  result r;
  r.code = response.result();
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!r.message.empty()) r.message += "\n";
    r.message += response.lines(i).message();
    for (int j = 0; j < response.lines(i).perf_size(); ++j) r.perf += response.lines(i).perf(j).alias() + ";";
  }
  return r;
}

result run_tcp(const std::vector<std::string> &args) { return run(&check_net::check_tcp, args); }
result run_ssh(const std::vector<std::string> &args) { return run(&check_net::check_ssh, args); }

// The detail syntax every test below renders, so the assertion reads the
// result word straight off the message.
const std::string kDetail = "detail-syntax=${host}:${port} ${result}";
const std::string kShowAll = "show-all";

}  // namespace

// ============================================================================
// Argument validation: refused before any connection is attempted
// ============================================================================

TEST(CheckTcpArgs, NoHostIsRejected) {
  const result r = run_tcp({"port=80"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("No host specified", r.message);
}

TEST(CheckTcpArgs, NoPortIsRejected) {
  const result r = run_tcp({"host=127.0.0.1"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("No port specified", r.message);
}

TEST(CheckTcpArgs, UnknownServicePresetIsRejected) {
  const result r = run_tcp({"host=127.0.0.1", "service=gopher"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("Unknown service preset: gopher", r.message);
}

TEST(CheckTcpArgs, UnknownStarttlsProtocolIsRejected) {
  const result r = run_tcp({"host=127.0.0.1", "starttls=carrier-pigeon"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_NE(std::string::npos, r.message.find("Unknown starttls protocol: carrier-pigeon")) << r.message;
  // The list of supported protocols comes from the preset table.
  EXPECT_NE(std::string::npos, r.message.find("smtp")) << r.message;
}

TEST(CheckTcpArgs, StarttlsAndServicePresetAreMutuallyExclusive) {
  const result r = run_tcp({"host=127.0.0.1", "starttls=smtp", "service=smtp"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_NE(std::string::npos, r.message.find("cannot be combined with the 'SMTP' service preset")) << r.message;
}

TEST(CheckTcpArgs, SniWithoutTlsIsRejected) {
  const result r = run_tcp({"host=127.0.0.1", "port=80", "sni=www.example.com"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_NE(std::string::npos, r.message.find("sni= only applies to a TLS connection")) << r.message;
}

TEST(CheckTcpArgs, InvalidAddressFamilyIsRejected) {
  const result r = run_tcp({"host=127.0.0.1", "port=80", "address-family=ipx"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("Invalid address-family: ipx (expected any, ipv4 or ipv6)", r.message);
}

TEST(CheckTcpArgs, BlankEntriesInHostsListAreIgnored) {
  // "hosts= , " splits into nothing but blanks, which leaves no host at all.
  const result r = run_tcp({"hosts= , ", "port=80"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("No host specified", r.message);
}

TEST(CheckSshArgs, HasNoServiceOption) {
  // check_ssh forces the SSH preset, so `service` is not one of its options.
  const result r = run_ssh({"host=127.0.0.1", "service=ftp"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_NE(std::string::npos, r.message.find("service")) << r.message;
}

// ============================================================================
// Connecting
// ============================================================================

TEST(CheckTcpConnect, PlainConnectIsOk) {
  scripted_server server([](tcp::socket &s) { drain(s); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), kDetail, kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("127.0.0.1:" + server.port() + " ok")) << r.message;
  // time carries perfdata, keyed by the perf syntax ${host}_${port}.
  EXPECT_NE(std::string::npos, r.perf.find("127.0.0.1_" + server.port())) << r.perf;
}

TEST(CheckTcpConnect, RefusedConnectionIsCritical) {
  const std::string port = closed_port();
  const result r = run_tcp({"host=127.0.0.1", "port=" + port, kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("127.0.0.1:" + port + " refused")) << r.message;
}

TEST(CheckTcpConnect, NoAddressInRequestedFamilyIsResolveFailed) {
  // An IPv6 literal has no IPv4 address: resolve_failed, without any DNS.
  // (Not the other way round: macOS answers an IPv6 lookup of 127.0.0.1 with
  // the v4-mapped ::ffff:127.0.0.1.)
  const result r = run_tcp({"host=::1", "port=80", "address-family=ipv4", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("resolve_failed")) << r.message;
}

TEST(CheckTcpConnect, EveryHostIsChecked) {
  // Two hosts, one comma list: both connections reach the same listener.
  scripted_server server(std::vector<script_type>{[](tcp::socket &s) { drain(s); }, [](tcp::socket &s) { drain(s); }});
  const result r = run_tcp({"hosts=127.0.0.1, 127.0.0.1", "port=" + server.port(), kDetail, kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  const std::string entry = "127.0.0.1:" + server.port() + " ok";
  const std::size_t first = r.message.find(entry);
  ASSERT_NE(std::string::npos, first) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(entry, first + entry.size())) << r.message;
}

// ============================================================================
// send / expect
// ============================================================================

TEST(CheckTcpExpect, PayloadIsSentAndResponseMatches) {
  std::string received;
  {
    scripted_server server([&received](tcp::socket &s) {
      received = read_line(s);
      send(s, "PONG from test\r\n");
      drain(s);
    });
    const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "send=PING\n", "expect=PONG", kDetail, kShowAll});
    EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
    EXPECT_NE(std::string::npos, r.message.find(" ok")) << r.message;
  }
  EXPECT_EQ("PING\n", received);
}

TEST(CheckTcpExpect, ResponseIsTrimmedAndExposedAsKeyword) {
  scripted_server server([](tcp::socket &s) {
    send(s, "  hello world \r\n");
    drain(s);
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "expect=hello", "detail-syntax=[${response}]", kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("[hello world]")) << r.message;
}

TEST(CheckTcpExpect, MismatchIsNoMatch) {
  scripted_server server([](tcp::socket &s) {
    send(s, "500 go away\r\n");
    drain(s);
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "expect=220", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" no_match")) << r.message;
}

TEST(CheckTcpExpect, SilentPeerIsReadTimeout) {
  scripted_server server([](tcp::socket &s) { drain(s); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "expect=anything", "timeout=200", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" read_timeout")) << r.message;
}

TEST(CheckTcpExpect, PeerThatHangsUpEarlyIsEvaluatedNotFailed) {
  // A clean close is end of data, not a read failure: what arrived (nothing)
  // is matched, and nothing does not contain the expected string.
  scripted_server server([](tcp::socket &s) { s.close(); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "expect=x", "timeout=2000", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" no_match")) << r.message;
}

TEST(CheckTcpExpect, PartialGreetingThenCloseStillMatches) {
  scripted_server server([](tcp::socket &s) {
    send(s, "220 ready");
    s.close();
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "expect=220", kDetail, kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
}

TEST(CheckTcpExpect, PlainConnectionDoesNotCoverRequiredSans) {
  // No TLS means no certificate, so a name required through sans= is missing:
  // the check must not report ok having asserted nothing.
  scripted_server server([](tcp::socket &s) { drain(s); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "sans=www.example.com", "detail-syntax=${result} ${missing_sans}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("san_missing www.example.com")) << r.message;
}

// ============================================================================
// Service presets
// ============================================================================

TEST(CheckTcpService, SmtpGreetingMatchesPreset) {
  scripted_server server([](tcp::socket &s) {
    send(s, "220 mail.example ESMTP ready\r\n");
    drain(s);
  });
  // port= overrides the preset's 25; the greeting regex still applies.
  const result r = run_tcp({"host=127.0.0.1", "service=smtp", "port=" + server.port(), kDetail, kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
}

TEST(CheckTcpService, WrongGreetingIsNoMatch) {
  scripted_server server([](tcp::socket &s) {
    send(s, "-ERR not a pop server\r\n");
    drain(s);
  });
  const result r = run_tcp({"host=127.0.0.1", "service=POP", "port=" + server.port(), kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" no_match")) << r.message;
}

TEST(CheckTcpService, PresetLookupIsCaseInsensitive) {
  ASSERT_NE(nullptr, check_net::find_service_preset("imap"));
  EXPECT_EQ(143, check_net::find_service_preset("Imap")->port);
  EXPECT_TRUE(check_net::find_service_preset("simap")->tls);
  EXPECT_EQ(nullptr, check_net::find_service_preset("gopher"));
}

// ============================================================================
// check_ssh
// ============================================================================

TEST(CheckSsh, BannerIsParsedIntoKeywords) {
  scripted_server server([](tcp::socket &s) {
    send(s, "SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5\r\n");
    drain(s);
  });
  const result r =
      run_ssh({"host=127.0.0.1", "port=" + server.port(), "detail-syntax=${result} ${protocol} ${software} ${software_version} ${comments}", kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("ok 2.0 OpenSSH 9.6p1 Ubuntu-3ubuntu13.5")) << r.message;
}

TEST(CheckSsh, ProtocolThresholdCatchesSshV1) {
  scripted_server server([](tcp::socket &s) {
    send(s, "SSH-1.5-ancient\r\n");
    drain(s);
  });
  const result r = run_ssh({"host=127.0.0.1", "port=" + server.port(), "critical=protocol_major < 2", "detail-syntax=${protocol_major}.${protocol_minor}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("1.5")) << r.message;
}

TEST(CheckSsh, NonSshServiceIsNoMatch) {
  scripted_server server([](tcp::socket &s) {
    send(s, "HTTP/1.1 400 Bad Request\r\n\r\n");
    drain(s);
  });
  const result r = run_ssh({"host=127.0.0.1", "port=" + server.port(), kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" no_match")) << r.message;
}

// ============================================================================
// STARTTLS negotiation (the plaintext half; each of these stops before TLS)
// ============================================================================

TEST(CheckTcpStarttls, SmtpServerRefusingStarttls) {
  std::vector<std::string> commands;
  {
    scripted_server server([&commands](tcp::socket &s) {
      send(s, "220 mail.example ESMTP\r\n");
      commands.push_back(read_line(s));
      send(s, "250-mail.example\r\n250 SIZE 1000\r\n");
      commands.push_back(read_line(s));
      send(s, "454 TLS not available\r\n");
      drain(s);
    });
    const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=smtp", kDetail});
    EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
    EXPECT_NE(std::string::npos, r.message.find(" starttls_refused")) << r.message;
  }
  ASSERT_EQ(2u, commands.size());
  EXPECT_EQ("EHLO nscp\r\n", commands[0]);
  EXPECT_EQ("STARTTLS\r\n", commands[1]);
}

TEST(CheckTcpStarttls, ServerHangingUpMidNegotiationIsDisconnected) {
  scripted_server server([](tcp::socket &s) {
    send(s, "220 mail.example ESMTP\r\n");
    read_line(s);
    s.close();
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=smtp", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" starttls_disconnected")) << r.message;
}

TEST(CheckTcpStarttls, SilentServerIsTimeout) {
  scripted_server server([](tcp::socket &s) { drain(s); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=pop3", "timeout=200", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" starttls_timeout")) << r.message;
}

TEST(CheckTcpStarttls, ServerFloodingWithoutNewlineOverflows) {
  // A greeting that never ends is a byte budget problem, not a time one.
  scripted_server server([](tcp::socket &s) {
    const std::string chunk(16 * 1024, 'x');
    for (int i = 0; i < 8; ++i) send(s, chunk);
    drain(s);
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=smtp", "timeout=5000", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" starttls_overflow")) << r.message;
}

TEST(CheckTcpStarttls, PostgresWithoutTlsIsRefused) {
  std::string request;
  {
    scripted_server server([&request](tcp::socket &s) {
      char buf[8];
      boost::system::error_code ec;
      boost::asio::read(s, boost::asio::buffer(buf), ec);
      request.assign(buf, sizeof(buf));
      send(s, "N");
      drain(s);
    });
    const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=postgres", kDetail});
    EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
    EXPECT_NE(std::string::npos, r.message.find(" starttls_refused")) << r.message;
  }
  // SSLRequest: length 8, then the magic code 80877103 (0x04d2162f).
  EXPECT_EQ(std::string("\x00\x00\x00\x08\x04\xd2\x16\x2f", 8), request);
}

TEST(CheckTcpStarttls, PostgresServerThatHangsUpIsDisconnected) {
  scripted_server server([](tcp::socket &s) {
    char buf[8];
    boost::system::error_code ec;
    boost::asio::read(s, boost::asio::buffer(buf), ec);
    s.close();
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=postgres", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" starttls_disconnected")) << r.message;
}

TEST(CheckTcpStarttls, LdapServerThatNeverAnswersTimesOut) {
  scripted_server server([](tcp::socket &s) { drain(s); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=ldap", "timeout=200", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" starttls_timeout")) << r.message;
}

TEST(CheckTcpStarttls, MysqlServerThatHangsUpBeforeHandshakeIsDisconnected) {
  scripted_server server([](tcp::socket &s) { s.close(); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=mysql", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" starttls_disconnected")) << r.message;
}

#ifdef USE_SSL

// ============================================================================
// TLS: a real handshake against a certificate minted for the test
// ============================================================================

TEST(CheckTcpTls, HandshakeWithoutVerificationReportsCertificate) {
  test_identity id;
  scripted_server server(tls_server(id));
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "verify=none",
                            "detail-syntax=${result} cn=${cert_cn} self=${cert_self_signed} has=${has_certificate} sans=${cert_sans}", kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("ok cn=localhost self=1 has=1")) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("localhost", r.message.find("sans="))) << r.message;
}

TEST(CheckTcpTls, ExpiryDaysAreExposedAndThresholdable) {
  test_identity id(10);
  scripted_server server(tls_server(id));
  const result r =
      run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "verify=none", "warning=ssl_expiry_days < 30", "detail-syntax=${ssl_expiry_days}"});
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.code) << r.message;
  // 10 days out; 9 if the day boundary falls inside the second the test spans.
  EXPECT_TRUE(r.message == "WARNING: 10" || r.message == "WARNING: 9") << r.message;
}

TEST(CheckTcpTls, UntrustedChainFailsTheHandshakeButKeepsTheCertificate) {
  // verify=peer (the default) with no trust anchor for a self-signed
  // certificate: the handshake fails, and the certificate it failed over is
  // still reported, along with OpenSSL's reason.
  test_identity id;
  scripted_server server(tls_server(id));
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "sni=localhost", "detail-syntax=${result} ${cert_cn} [${cert_verify}]"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("tls_handshake_failed localhost [")) << r.message;
  EXPECT_EQ(std::string::npos, r.message.find("[ok]")) << r.message;
}

TEST(CheckTcpTls, TrustedChainVerifiesThroughCaBundle) {
  test_identity id;
  const std::string ca = id.write_pem();
  {
    scripted_server server(tls_server(id));
    const result r =
        run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "ca=" + ca, "sni=localhost", "detail-syntax=${result} ${cert_verify}", kShowAll});
    EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
    EXPECT_NE(std::string::npos, r.message.find("ok ok")) << r.message;
  }
  boost::filesystem::remove(ca);
}

TEST(CheckTcpTls, MissingSanIsReportedOverAnOkConversation) {
  test_identity id;
  scripted_server server(tls_server(id));
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "verify=none", "sans=localhost,www.example.com",
                            "detail-syntax=${result} missing=${missing_sans}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("san_missing missing=www.example.com")) << r.message;
}

TEST(CheckTcpTls, ConversationRunsOverTheEncryptedStream) {
  test_identity id;
  scripted_server server(tls_server(id, [](boost::asio::ssl::stream<tcp::socket &> &stream) {
    char buf[5];
    boost::system::error_code ec;
    boost::asio::read(stream, boost::asio::buffer(buf), ec);
    if (!ec && std::string(buf, 5) == "HELO\n") boost::asio::write(stream, boost::asio::buffer(std::string("secret hello\n")), ec);
  }));
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "verify=none", "send=HELO\n", "expect=secret", kDetail, kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
}

TEST(CheckTcpTls, PlaintextPeerFailsTheHandshake) {
  scripted_server server([](tcp::socket &s) {
    send(s, "220 this is not TLS\r\n");
    drain(s);
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "verify=none", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" tls_handshake_failed")) << r.message;
}

TEST(CheckTcpTls, SilentPeerIsHandshakeTimeout) {
  scripted_server server([](tcp::socket &s) { drain(s); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "verify=none", "timeout=200", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" tls_handshake_timeout")) << r.message;
}

TEST(CheckTcpTls, UnreadableCaBundleIsAnErrorWithoutTheOsReason) {
  // ca= is caller supplied; the reason OpenSSL gives for not loading it is an
  // oracle on the agent's filesystem and must not reach the result.
  scripted_server server([](tcp::socket &s) { drain(s); });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "ssl=true", "ca=/nonexistent/nscp/ca.pem", kDetail});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find(" error: ")) << r.message;
  EXPECT_EQ(std::string::npos, r.message.find("No such file")) << r.message;
}

TEST(CheckTcpTls, StarttlsUpgradeThenHandshake) {
  // The whole opportunistic path: SMTP negotiates in the clear, then the same
  // socket carries the TLS handshake.
  test_identity id;
  scripted_server server([&id](tcp::socket &s) {
    send(s, "220 mail.example ESMTP\r\n");
    read_line(s);
    send(s, "250 mail.example\r\n");
    read_line(s);
    send(s, "220 go ahead\r\n");
    tls_server(id)(s);
  });
  const result r = run_tcp({"host=127.0.0.1", "port=" + server.port(), "starttls=smtp", "verify=none", "detail-syntax=${result} ${cert_cn}", kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("ok localhost")) << r.message;
}

TEST(CheckTcpTls, ImplicitTlsServicePresetTurnsTlsOn) {
  test_identity id;
  scripted_server server(tls_server(id, [](boost::asio::ssl::stream<tcp::socket &> &stream) {
    boost::system::error_code ec;
    boost::asio::write(stream, boost::asio::buffer(std::string("+OK POP3 ready\r\n")), ec);
  }));
  const result r = run_tcp({"host=127.0.0.1", "service=spop", "port=" + server.port(), "verify=none", "detail-syntax=${result} ${has_certificate}", kShowAll});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("ok 1")) << r.message;
}

#endif  // USE_SSL
