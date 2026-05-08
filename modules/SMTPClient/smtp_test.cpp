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

#include <string>

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
