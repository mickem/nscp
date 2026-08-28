// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for the security-critical helpers in the SMTP client.
 *
 * These exercise the pure transformations that protect against:
 *   - CRLF injection in envelope addresses (validate_address)
 *   - CRLF injection in header values     (sanitise_header)
 *   - DATA payload smuggling via leading "."  (dot_stuff_and_crlf)
 *
 * The end-to-end SMTP flow (TCP, STARTTLS, AUTH, message delivery) is
 * covered by tests/smtp/run-test.bat which spins up an aiosmtpd server
 * in Docker. Anything that can be tested without IO lives here.
 */

#include "smtp.hpp"

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using smtp::smtp_exception;
using smtp::detail::dot_stuff_and_crlf;
using smtp::detail::sanitise_header;
using smtp::detail::validate_address;

// =============================================================================
// validate_address
// =============================================================================

TEST(SmtpValidateAddress, AcceptsTypicalMailbox) {
  EXPECT_NO_THROW(validate_address("alice@example.com", "from"));
  EXPECT_NO_THROW(validate_address("alerts+nscp@example.com", "from"));
  EXPECT_NO_THROW(validate_address("a.b.c@sub.example.com", "from"));
}

TEST(SmtpValidateAddress, RejectsEmpty) { EXPECT_THROW(validate_address("", "from"), smtp_exception); }

TEST(SmtpValidateAddress, RejectsBareCR) { EXPECT_THROW(validate_address(std::string("foo\rbar@example.com"), "from"), smtp_exception); }

TEST(SmtpValidateAddress, RejectsBareLF) { EXPECT_THROW(validate_address(std::string("foo\nbar@example.com"), "from"), smtp_exception); }

TEST(SmtpValidateAddress, RejectsCRLFInjection) {
  // Classic SMTP smuggling attempt - the attacker embeds a full new line
  // hoping the server treats it as another envelope command.
  EXPECT_THROW(validate_address(std::string("alice@example.com\r\nBcc: evil@x.com"), "to"), smtp_exception);
}

TEST(SmtpValidateAddress, RejectsEmbeddedNul) { EXPECT_THROW(validate_address(std::string("foo\0bar@example.com", 19), "from"), smtp_exception); }

TEST(SmtpValidateAddress, RejectsTab) { EXPECT_THROW(validate_address(std::string("foo\tbar@example.com"), "from"), smtp_exception); }

TEST(SmtpValidateAddress, RejectsAngleBrackets) {
  // The caller wraps the address in <...> when emitting MAIL FROM / RCPT TO,
  // so accepting an address that already contains them would let the caller
  // smuggle a second envelope.
  EXPECT_THROW(validate_address("<alice@example.com>", "from"), smtp_exception);
  EXPECT_THROW(validate_address("alice@example.com>", "from"), smtp_exception);
  EXPECT_THROW(validate_address("alice<@example.com", "from"), smtp_exception);
}

// =============================================================================
// sanitise_header
// =============================================================================

TEST(SmtpSanitiseHeader, PassesPlainText) {
  EXPECT_EQ(sanitise_header("Hello there"), "Hello there");
  EXPECT_EQ(sanitise_header(""), "");
}

TEST(SmtpSanitiseHeader, StripsCR) { EXPECT_EQ(sanitise_header(std::string("foo\rbar")), "foobar"); }

TEST(SmtpSanitiseHeader, StripsLF) { EXPECT_EQ(sanitise_header(std::string("foo\nbar")), "foobar"); }

TEST(SmtpSanitiseHeader, StripsCRLFInjectionAttempt) {
  // Subject smuggling: the attacker tries to add a Bcc header. After
  // sanitisation the bytes survive but flatten into the subject - the
  // injected header line is gone.
  const std::string injected = "alert\r\nBcc: evil@example.com";
  const std::string clean = sanitise_header(injected);
  EXPECT_EQ(clean, "alertBcc: evil@example.com");
  EXPECT_EQ(clean.find('\r'), std::string::npos);
  EXPECT_EQ(clean.find('\n'), std::string::npos);
}

TEST(SmtpSanitiseHeader, StripsNul) { EXPECT_EQ(sanitise_header(std::string("foo\0bar", 7)), "foobar"); }

TEST(SmtpSanitiseHeader, KeepsTabsAndOtherPrintable) {
  // A tab in a header is unusual but technically allowed (folding white
  // space). We don't strip it because RFC 5322 considers it valid.
  EXPECT_EQ(sanitise_header(std::string("foo\tbar")), "foo\tbar");
  EXPECT_EQ(sanitise_header("with spaces and !@#$%^&*()"), "with spaces and !@#$%^&*()");
}

// =============================================================================
// dot_stuff_and_crlf
// =============================================================================

TEST(SmtpDotStuff, SimpleBodyGetsCrlfTerminator) {
  // A single line without trailing newline should arrive as line + CRLF.
  EXPECT_EQ(dot_stuff_and_crlf("hello"), "hello\r\n");
}

TEST(SmtpDotStuff, NormalisesLfToCrlf) { EXPECT_EQ(dot_stuff_and_crlf("line1\nline2\nline3"), "line1\r\nline2\r\nline3\r\n"); }

TEST(SmtpDotStuff, NormalisesCrToCrlf) {
  // Lone CR (old-Mac line endings) - should also normalise.
  EXPECT_EQ(dot_stuff_and_crlf("line1\rline2"), "line1\r\nline2\r\n");
}

TEST(SmtpDotStuff, PreservesCrlf) { EXPECT_EQ(dot_stuff_and_crlf("line1\r\nline2\r\n"), "line1\r\nline2\r\n"); }

TEST(SmtpDotStuff, EmptyBodyEmitsNothing) {
  // Empty body should produce empty output (caller appends ".\r\n" itself).
  EXPECT_EQ(dot_stuff_and_crlf(""), "");
}

TEST(SmtpDotStuff, DoublesLeadingDotOnFirstLine) {
  // A line that starts with "." would otherwise be interpreted by the
  // server as the end-of-data marker.
  EXPECT_EQ(dot_stuff_and_crlf(".begin"), "..begin\r\n");
}

TEST(SmtpDotStuff, DoublesLeadingDotOnSubsequentLines) { EXPECT_EQ(dot_stuff_and_crlf("normal\n.dotline\nnormal2"), "normal\r\n..dotline\r\nnormal2\r\n"); }

TEST(SmtpDotStuff, DoublesLeadingDotOnMultipleConsecutiveLines) { EXPECT_EQ(dot_stuff_and_crlf(".one\n.two\n.three"), "..one\r\n..two\r\n..three\r\n"); }

TEST(SmtpDotStuff, EndOfDataMarkerCannotBeSmuggled) {
  // The literal end-of-data line is "<CRLF>.<CRLF>". An attacker who
  // controls the body could try to terminate DATA early and inject a new
  // RSET/MAIL FROM/etc. Dot-stuffing prevents this: the leading "." gets
  // doubled to "..", so the server never sees the unescaped marker.
  const std::string evil = "innocent text\r\n.\r\nMAIL FROM:<attacker@x.com>\r\n";
  const std::string out = dot_stuff_and_crlf(evil);
  // Look for the unescaped "<CRLF>.<CRLF>" sequence anywhere in the output.
  // The post-dot-stuffing sequence should be "<CRLF>..<CRLF>".
  EXPECT_EQ(out.find("\r\n.\r\n"), std::string::npos);
  EXPECT_NE(out.find("\r\n..\r\n"), std::string::npos);
}

TEST(SmtpDotStuff, DotInTheMiddleOfALineIsUntouched) {
  // Only leading dots get doubled - dots in the middle pass through.
  EXPECT_EQ(dot_stuff_and_crlf("server.example.com is up"), "server.example.com is up\r\n");
}

// =============================================================================
// STARTTLS response injection
// =============================================================================
//
// These drive the real smtp::send() against a scripted plaintext server. No
// TLS server is needed: everything under test happens before the handshake,
// and the handshake against a non-TLS peer fails in a distinguishable way,
// which is exactly what the negative control wants.

namespace {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// A single-connection plaintext server replaying a fixed script: write a
// canned response, read one command line, write the next, and so on. Each
// response goes out in one write(), so a response carrying more than one line
// reaches the client as a single segment - which is how a real injection
// arrives.
// `delay` is slept before each response, to model a server that answers
// slowly but never hangs outright.
class scripted_server {
 public:
  explicit scripted_server(std::vector<std::string> responses, std::chrono::milliseconds delay = std::chrono::milliseconds(0))
      : acceptor_(io_, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)), responses_(std::move(responses)), delay_(delay) {
    port_ = acceptor_.local_endpoint().port();
    thread_ = std::thread([this] { serve(); });
  }
  ~scripted_server() {
    if (thread_.joinable()) thread_.join();
  }

  std::string port() const { return std::to_string(port_); }

 private:
  void serve() {
    boost::system::error_code ec;
    tcp::socket sock(io_);
    acceptor_.accept(sock, ec);
    if (ec) return;
    asio::streambuf buf;
    for (std::size_t i = 0; i < responses_.size(); ++i) {
      if (i > 0) {
        asio::read_until(sock, buf, "\r\n", ec);
        if (ec) return;
        std::istream is(&buf);
        std::string command;
        std::getline(is, command);
      }
      if (delay_.count() > 0) std::this_thread::sleep_for(delay_);
      asio::write(sock, asio::buffer(responses_[i]), ec);
      if (ec) return;
    }
    // Give the client time to read the last response before the socket goes
    // away, so it sees the script rather than a reset.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  asio::io_context io_;
  tcp::acceptor acceptor_;
  std::vector<std::string> responses_;
  std::chrono::milliseconds delay_;
  unsigned short port_ = 0;
  std::thread thread_;
};

const char *kBanner = "220 mx.example.com ESMTP\r\n";
const char *kEhloWithStarttls = "250-mx.example.com\r\n250-STARTTLS\r\n250 HELP\r\n";

// Run a STARTTLS submission against a scripted server and return the
// resulting error message. Every case here fails somewhere - the server does
// not speak TLS - so the message is the assertion target.
std::string starttls_failure(const std::string &starttls_reply, std::chrono::milliseconds delay = std::chrono::milliseconds(0),
                             int timeout_seconds = 5) {
  scripted_server server({kBanner, kEhloWithStarttls, starttls_reply}, delay);

  smtp::connection_config cfg;
  cfg.server = "127.0.0.1";
  cfg.port = server.port();
  cfg.security = "starttls";
  cfg.insecure_skip_verify = true;
  cfg.timeout_seconds = timeout_seconds;

  smtp::message msg;
  msg.from = "agent@example.com";
  msg.to = "ops@example.com";
  msg.subject = "hello";
  msg.body = "body";

  try {
    smtp::send(cfg, msg);
  } catch (const smtp_exception &e) {
    return e.what();
  }
  return "<no exception>";
}

}  // namespace

TEST(SmtpStarttls, RefusesDataPipelinedIntoTheStarttlsGreeting) {
  // The server appends forged replies to its "220 go ahead" in the same
  // segment. Those bytes are cleartext and attacker-controlled, but they would
  // be consumed by the reads that happen after the handshake - so they would be
  // trusted as if they had arrived inside the TLS session. Refuse the session.
  const std::string error = starttls_failure("220 Go ahead\r\n250-mx.example.com\r\n250 OK\r\n");

  EXPECT_NE(error.find("STARTTLS response injection"), std::string::npos) << error;
}

TEST(SmtpStarttls, ForgedDeliveryAcknowledgementsAreNotAccepted) {
  // The payoff of the injection above: a full set of forged 2xx replies would
  // walk the client through MAIL/RCPT/DATA and have it report the notification
  // as delivered when nothing was ever sent. The submission must fail instead.
  const std::string error = starttls_failure(
      "220 Go ahead\r\n"
      "250-mx.example.com\r\n250 AUTH PLAIN\r\n"
      "250 sender ok\r\n"
      "250 recipient ok\r\n"
      "354 go\r\n"
      "250 queued as 12345\r\n");

  EXPECT_NE(error.find("STARTTLS response injection"), std::string::npos) << error;
}

TEST(SmtpStarttls, AWellBehavedGreetingReachesTheHandshake) {
  // Negative control: with nothing pipelined the guard must stay out of the
  // way. The scripted server does not speak TLS, so the session still fails -
  // but at the handshake, which proves the guard did not fire.
  const std::string error = starttls_failure("220 Go ahead\r\n");

  EXPECT_EQ(error.find("STARTTLS response injection"), std::string::npos) << error;
  EXPECT_NE(error.find("TLS handshake (STARTTLS)"), std::string::npos) << error;
}

// =============================================================================
// Timeout budget
// =============================================================================

TEST(SmtpTimeout, TheTimeoutBoundsTheWholeSubmissionNotEachOperation) {
  // The server answers every command in 400ms - comfortably inside any single
  // deadline, but three of them cannot fit in a one second budget. With a
  // per-operation deadline each read would get its own second, the session
  // would walk all the way to the handshake, and an operator who asked to give
  // up after a second would instead wait a multiple of it.
  const std::string error = starttls_failure("220 Go ahead\r\n", std::chrono::milliseconds(400), 1);

  EXPECT_NE(error.find("timed out"), std::string::npos) << error;
}

// =============================================================================
// CA bundle
// =============================================================================

TEST(SmtpCaBundle, AnUnreadableBundleFailsTheSubmission) {
  // Silently continuing with an unloadable CA would leave verify_peer running
  // against an empty trust store, which fails the handshake with a confusing
  // "unable to get local issuer certificate". Name the real problem instead.
  smtp::connection_config cfg;
  cfg.server = "127.0.0.1";
  cfg.port = "1";
  cfg.security = "tls";
  cfg.ca_path = "/nonexistent/no-such-ca-bundle.pem";

  smtp::message msg;
  msg.from = "agent@example.com";
  msg.to = "ops@example.com";
  msg.body = "body";

  try {
    smtp::send(cfg, msg);
    FAIL() << "expected the missing CA bundle to fail the submission";
  } catch (const smtp_exception &e) {
    EXPECT_NE(std::string(e.what()).find("failed to load CA bundle"), std::string::npos) << e.what();
  }
}

TEST(SmtpCaBundle, TheBundleIsIgnoredWhenVerificationIsWaived) {
  // insecure-skip-verify is for self-signed labs, where ${ca-path} may well
  // not resolve to anything. Loading the bundle only in the verifying branch
  // keeps a missing file from breaking a session that never wanted it.
  smtp::connection_config cfg;
  cfg.server = "127.0.0.1";
  cfg.port = "1";
  cfg.security = "tls";
  cfg.ca_path = "/nonexistent/no-such-ca-bundle.pem";
  cfg.insecure_skip_verify = true;
  cfg.timeout_seconds = 5;

  smtp::message msg;
  msg.from = "agent@example.com";
  msg.to = "ops@example.com";
  msg.body = "body";

  try {
    smtp::send(cfg, msg);
    FAIL() << "expected the connection to port 1 to fail";
  } catch (const smtp_exception &e) {
    // It must get as far as the connection - i.e. past the CA setup entirely.
    EXPECT_EQ(std::string(e.what()).find("failed to load CA bundle"), std::string::npos) << e.what();
    EXPECT_NE(std::string(e.what()).find("connect failed"), std::string::npos) << e.what();
  }
}

// =============================================================================
// EHLO capability parsing
// =============================================================================
//
// read_reply() joins the lines of a multi-line reply with '\n', so that is the
// shape these take.

using smtp::detail::auth_mechanisms;
using smtp::detail::has_capability;

namespace {
const char *kTypicalEhlo =
    "250-mx.example.com at your service\n"
    "250-SIZE 35882577\n"
    "250-8BITMIME\n"
    "250-STARTTLS\n"
    "250-AUTH LOGIN PLAIN XOAUTH2\n"
    "250 SMTPUTF8";
}  // namespace

TEST(SmtpCapabilities, FindsCapabilitiesOnTheirOwnLine) {
  EXPECT_TRUE(has_capability(kTypicalEhlo, "STARTTLS"));
  EXPECT_TRUE(has_capability(kTypicalEhlo, "8BITMIME"));
  EXPECT_TRUE(has_capability(kTypicalEhlo, "SIZE"));
  EXPECT_TRUE(has_capability(kTypicalEhlo, "SMTPUTF8"));
}

TEST(SmtpCapabilities, IsCaseInsensitive) {
  EXPECT_TRUE(has_capability("250 starttls", "STARTTLS"));
  EXPECT_TRUE(has_capability("250 STARTTLS", "starttls"));
}

TEST(SmtpCapabilities, DoesNotMatchTheGreetingText) {
  // The greeting is free text the server chooses. A host that names itself
  // after the capability must not be able to answer for it - with substring
  // matching, "starttls.example.com" advertised STARTTLS.
  EXPECT_FALSE(has_capability("250-starttls.example.com ESMTP\n250 SIZE 100", "STARTTLS"));
}

TEST(SmtpCapabilities, DoesNotMatchACapabilityParameter) {
  // AUTH's mechanism list is parameters, not capabilities of its own.
  EXPECT_FALSE(has_capability("250-mx.example.com\n250 AUTH PLAIN LOGIN", "PLAIN"));
  EXPECT_FALSE(has_capability("250-mx.example.com\n250 AUTH PLAIN LOGIN", "LOGIN"));
}

TEST(SmtpCapabilities, DoesNotMatchAPrefixOrSuffixOfAnotherCapability) {
  EXPECT_FALSE(has_capability("250 STARTTLSFOO", "STARTTLS"));
  EXPECT_FALSE(has_capability("250 XSTARTTLS", "STARTTLS"));
}

TEST(SmtpCapabilities, DoesNotMatchAcrossALineJoin) {
  // Replies used to be concatenated with no separator at all, so the tail of
  // one line and the head of the next formed tokens no server ever sent:
  // "250 X-START" + "250-TLS ..." read as "...X-START250-TLS...". The join is
  // now a newline and matching is per line, so neither half can conjure a
  // capability.
  EXPECT_FALSE(has_capability("250-X-START\n250-TLS ok\n250 SIZE 1", "STARTTLS"));
}

TEST(SmtpCapabilities, AnEmptyOrShortReplyAdvertisesNothing) {
  EXPECT_FALSE(has_capability("", "STARTTLS"));
  EXPECT_FALSE(has_capability("250", "STARTTLS"));
}

TEST(SmtpAuthMechanisms, ListsTheAdvertisedMechanisms) {
  const std::vector<std::string> mechs = auth_mechanisms(kTypicalEhlo);
  EXPECT_EQ(mechs, (std::vector<std::string>{"LOGIN", "PLAIN", "XOAUTH2"}));
}

TEST(SmtpAuthMechanisms, UppercasesWhateverTheServerSent) {
  EXPECT_EQ(auth_mechanisms("250 AUTH plain login"), (std::vector<std::string>{"PLAIN", "LOGIN"}));
}

TEST(SmtpAuthMechanisms, IsEmptyWhenAuthIsNotAdvertised) {
  EXPECT_TRUE(auth_mechanisms("250-mx.example.com\n250 STARTTLS").empty());
  // A greeting mentioning the word must not be read as an AUTH capability.
  EXPECT_TRUE(auth_mechanisms("250 AUTHORITY.example.com ESMTP").empty());
}

TEST(SmtpAuthMechanisms, IsEmptyForAnAuthCapabilityWithNoMechanisms) { EXPECT_TRUE(auth_mechanisms("250 AUTH").empty()); }
