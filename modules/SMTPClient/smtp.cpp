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

#include "smtp.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <bytes/base64.hpp>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace smtp {
namespace {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using ssl_stream = asio::ssl::stream<tcp::socket>;

// ---------------------------------------------------------------------------
// Tiny synchronous IO with deadline
// ---------------------------------------------------------------------------
//
// We need synchronous read/write but must also enforce a timeout so a stuck
// peer cannot hang the agent forever. Boost.Asio synchronous calls do not
// take a timeout - the canonical workaround is to issue the operation as
// async, run() the io_service for the duration of a deadline timer, and
// cancel the socket when the timer fires.

class sync_io {
 public:
  sync_io(asio::io_service& io, tcp::socket& s, int timeout_seconds) : io_(io), socket_ref_(s), tls_stream_(nullptr), timeout_(timeout_seconds) {}

  void use_tls(ssl_stream& s) { tls_stream_ = &s; }

  // Connect to the first endpoint that succeeds, with a deadline.
  void connect(tcp::resolver::iterator endpoints) {
    boost::system::error_code ec = asio::error::host_not_found;
    auto end = tcp::resolver::iterator{};
    while (ec && endpoints != end) {
      socket_ref_.close();
      run_with_deadline([&](auto&& done) { socket_ref_.async_connect(*endpoints, [done = std::move(done)](const boost::system::error_code& e) { done(e); }); },
                        ec);
      if (ec) ++endpoints;
    }
    if (ec) throw smtp_exception("connect failed: " + ec.message());
  }

  // Write a string. CRLF must already be in `data`.
  void write(const std::string& data) {
    boost::system::error_code ec;
    run_with_deadline(
        [&](auto&& done) {
          if (tls_stream_) {
            asio::async_write(*tls_stream_, asio::buffer(data), [done = std::move(done)](const boost::system::error_code& e, std::size_t) { done(e); });
          } else {
            asio::async_write(socket_ref_, asio::buffer(data), [done = std::move(done)](const boost::system::error_code& e, std::size_t) { done(e); });
          }
        },
        ec);
    if (ec) throw smtp_exception("write failed: " + ec.message());
  }

  // Read one CRLF-terminated SMTP reply. Multi-line replies are concatenated
  // (the caller gets every line up to and including the final "NNN <space>").
  std::string read_reply() {
    std::string out;
    while (true) {
      const std::string line = read_line();
      out += line;
      // RFC 5321 4.2: each reply line is "NNN-text" for continuation,
      // "NNN text" for the final line. A line shorter than 4 bytes is a
      // protocol error.
      if (line.size() < 4) {
        throw smtp_exception("malformed SMTP reply: '" + line + "'");
      }
      if (line[3] == ' ') break;
      if (line[3] != '-') throw smtp_exception("malformed SMTP reply: '" + line + "'");
    }
    return out;
  }

 private:
  // Read one CRLF-terminated line. Strips the trailing CRLF.
  std::string read_line() {
    boost::system::error_code ec;
    std::size_t bytes = 0;
    run_with_deadline(
        [&](auto&& done) {
          if (tls_stream_) {
            asio::async_read_until(*tls_stream_, buf_, "\r\n", [done = std::move(done)](const boost::system::error_code& e, std::size_t n) { done(e, n); });
          } else {
            asio::async_read_until(socket_ref_, buf_, "\r\n", [done = std::move(done)](const boost::system::error_code& e, std::size_t n) { done(e, n); });
          }
        },
        ec, &bytes);
    if (ec) throw smtp_exception("read failed: " + ec.message());
    std::istream is(&buf_);
    std::string line;
    std::getline(is, line);  // strips '\n'
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
  }

  // Run a single async op until it completes or the deadline expires. Sets
  // `out_ec` to the operation's result, or operation_aborted on timeout.
  template <typename Init>
  void run_with_deadline(Init&& init, boost::system::error_code& out_ec, std::size_t* out_bytes = nullptr) {
    out_ec = asio::error::would_block;
    std::size_t bytes = 0;
    asio::deadline_timer timer(io_);
    timer.expires_from_now(boost::posix_time::seconds(timeout_));
    bool timed_out = false;
    timer.async_wait([&](const boost::system::error_code& e) {
      if (e == asio::error::operation_aborted) return;
      timed_out = true;
      // Cancel any outstanding I/O so run() returns.
      boost::system::error_code ignore;
      socket_ref_.cancel(ignore);
    });
    init([&](const boost::system::error_code& e, std::size_t n = 0) {
      out_ec = e;
      bytes = n;
      timer.cancel();
    });
    io_.reset();
    io_.run();
    if (timed_out) {
      out_ec = asio::error::timed_out;
    }
    if (out_bytes) *out_bytes = bytes;
  }

  asio::io_service& io_;
  tcp::socket& socket_ref_;
  ssl_stream* tls_stream_;
  asio::streambuf buf_;
  int timeout_;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void expect_status(const std::string& reply, char expected_first, const std::string& context) {
  if (reply.size() < 3 || reply[0] != expected_first) {
    throw smtp_exception(context + " unexpected reply: " + reply);
  }
}

bool reply_starts_with(const std::string& reply, const char* code) { return reply.size() >= 3 && reply.compare(0, 3, code) == 0; }

}  // namespace

// Security-critical pure helpers. Declared in smtp.hpp so the test suite
// can exercise them directly without standing up a real SMTP server.
namespace detail {

// CRLF injection guard for envelope addresses. Any control byte is
// rejected so the caller cannot smuggle a bare CR/LF that would split
// a line at the SMTP layer; angle brackets are rejected so the caller
// cannot smuggle their own envelope wrapping.
void validate_address(const std::string& addr, const char* what) {
  if (addr.empty()) {
    throw smtp_exception(std::string(what) + " is empty");
  }
  for (char c : addr) {
    const auto u = static_cast<unsigned char>(c);
    if (u < 0x20 || u == 0x7f) {
      throw smtp_exception(std::string(what) + " contains a control character");
    }
  }
  if (addr.find('<') != std::string::npos || addr.find('>') != std::string::npos) {
    throw smtp_exception(std::string(what) + " must not contain angle brackets");
  }
}

// Reject CR / LF / NUL in any header value so a header like
//   Subject: foo\r\nBcc: attacker
// cannot inject a hidden recipient.
std::string sanitise_header(const std::string& v) {
  std::string out;
  out.reserve(v.size());
  for (char c : v) {
    if (c == '\r' || c == '\n' || c == '\0') continue;
    out.push_back(c);
  }
  return out;
}

// RFC 5321 4.5.2 transparency: any line in DATA that starts with "." must
// be sent as ".." Also normalise lone CR / LF to CRLF so a body produced
// on Unix or Windows arrives with consistent line endings.
std::string dot_stuff_and_crlf(const std::string& body) {
  std::string out;
  out.reserve(body.size() + 32);
  bool at_line_start = true;
  for (std::size_t i = 0; i < body.size(); ++i) {
    const char c = body[i];
    if (at_line_start && c == '.') {
      out.push_back('.');
    }
    if (c == '\r') {
      // Skip; we will emit CRLF if a \n follows, otherwise a synthetic CRLF.
      if (i + 1 < body.size() && body[i + 1] == '\n') continue;
      out.append("\r\n");
      at_line_start = true;
    } else if (c == '\n') {
      out.append("\r\n");
      at_line_start = true;
    } else {
      out.push_back(c);
      at_line_start = false;
    }
  }
  if (!out.empty() && (out.size() < 2 || out.compare(out.size() - 2, 2, "\r\n") != 0)) {
    out.append("\r\n");
  }
  return out;
}

}  // namespace detail

namespace {  // re-open anon namespace for the rest of the helpers

std::string b64(const std::string& s) { return bytes::base64_encode(s); }

void do_auth(sync_io& io, const std::string& user, const std::string& pass, const std::string& ehlo_response) {
  // Prefer AUTH PLAIN when the server advertises it (most servers do).
  // Fall back to AUTH LOGIN otherwise. Both are TLS-only here because we
  // refuse to enter this function over an unencrypted channel - see
  // send().
  const bool plain =
      ehlo_response.find("AUTH") != std::string::npos && (ehlo_response.find("PLAIN") != std::string::npos || ehlo_response.find("LOGIN") == std::string::npos);
  if (plain) {
    // RFC 4616: "\0username\0password" base64-encoded.
    std::string sasl;
    sasl.push_back('\0');
    sasl += user;
    sasl.push_back('\0');
    sasl += pass;
    io.write("AUTH PLAIN " + b64(sasl) + "\r\n");
    const std::string r = io.read_reply();
    if (!reply_starts_with(r, "235")) throw smtp_exception("AUTH PLAIN rejected: " + r);
    return;
  }
  io.write("AUTH LOGIN\r\n");
  std::string r = io.read_reply();
  if (!reply_starts_with(r, "334")) throw smtp_exception("AUTH LOGIN not accepted: " + r);
  io.write(b64(user) + "\r\n");
  r = io.read_reply();
  if (!reply_starts_with(r, "334")) throw smtp_exception("AUTH LOGIN username rejected: " + r);
  io.write(b64(pass) + "\r\n");
  r = io.read_reply();
  if (!reply_starts_with(r, "235")) throw smtp_exception("AUTH LOGIN password rejected: " + r);
}

}  // namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

void send(const connection_config& cfg, const message& msg) {
  if (cfg.server.empty()) throw smtp_exception("smtp server hostname not configured");

  detail::validate_address(msg.from, "from");
  detail::validate_address(msg.to, "to");
  const std::string subject = detail::sanitise_header(msg.subject);

  // Validate security mode early so a typo does not silently fall through
  // to plain transport.
  const std::string sec = boost::algorithm::to_lower_copy(cfg.security);
  const bool tls_immediate = (sec == "tls" || sec == "ssl");
  const bool starttls = (sec == "starttls");
  const bool plain_only = (sec == "none");
  if (!tls_immediate && !starttls && !plain_only) {
    throw smtp_exception("invalid security mode '" + cfg.security + "' (expected none|starttls|tls)");
  }

  // SSL context. We default to TLS 1.2+ peer verification and let the
  // operator override with insecure_skip_verify for test servers / labs.
  asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
  ssl_ctx.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 | asio::ssl::context::no_tlsv1 |
                      asio::ssl::context::no_tlsv1_1);
  if (cfg.insecure_skip_verify) {
    ssl_ctx.set_verify_mode(asio::ssl::verify_none);
  } else {
    ssl_ctx.set_verify_mode(asio::ssl::verify_peer);
    ssl_ctx.set_default_verify_paths();
  }

  asio::io_service io;
  // The owning ssl_stream<tcp::socket> form keeps the underlying socket
  // accessible via next_layer(). We do plain SMTP reads/writes against
  // next_layer() before STARTTLS, then handshake() upgrades subsequent IO
  // to flow through `tls`.
  ssl_stream tls(io, ssl_ctx);
  if (!cfg.insecure_skip_verify) {
    tls.set_verify_callback(asio::ssl::rfc2818_verification(cfg.server));
  }

  sync_io conn(io, tls.next_layer(), cfg.timeout_seconds);

  // Resolve and connect.
  tcp::resolver resolver(io);
  tcp::resolver::query query(cfg.server, cfg.port);
  boost::system::error_code resolve_ec;
  auto endpoints = resolver.resolve(query, resolve_ec);
  if (resolve_ec) throw smtp_exception("DNS resolve failed: " + resolve_ec.message());
  conn.connect(endpoints);

  if (tls_immediate) {
    boost::system::error_code ec;
    tls.handshake(asio::ssl::stream_base::client, ec);
    if (ec) throw smtp_exception("TLS handshake failed: " + ec.message());
    conn.use_tls(tls);
  }

  // Banner.
  std::string r = conn.read_reply();
  expect_status(r, '2', "banner");

  const std::string ehlo_name = cfg.canonical_name.empty() ? std::string("localhost") : cfg.canonical_name;

  // EHLO.
  conn.write("EHLO " + ehlo_name + "\r\n");
  r = conn.read_reply();
  if (r.size() < 3 || r[0] != '2') {
    // Fall back to HELO for ancient relays.
    conn.write("HELO " + ehlo_name + "\r\n");
    r = conn.read_reply();
    expect_status(r, '2', "HELO");
  }
  std::string ehlo_caps = r;

  // STARTTLS upgrade.
  if (starttls) {
    if (ehlo_caps.find("STARTTLS") == std::string::npos) {
      throw smtp_exception("server did not advertise STARTTLS but security=starttls was requested");
    }
    conn.write("STARTTLS\r\n");
    r = conn.read_reply();
    expect_status(r, '2', "STARTTLS");
    boost::system::error_code ec;
    tls.handshake(asio::ssl::stream_base::client, ec);
    if (ec) throw smtp_exception("TLS handshake (STARTTLS) failed: " + ec.message());
    conn.use_tls(tls);
    // Re-EHLO over the secure channel; capabilities can change after TLS.
    conn.write("EHLO " + ehlo_name + "\r\n");
    r = conn.read_reply();
    expect_status(r, '2', "EHLO over TLS");
    ehlo_caps = r;
  }

  // AUTH if requested. Refuse to send credentials in clear - if the operator
  // configured a password but didn't request TLS, fail loud rather than leak
  // it.
  if (!cfg.username.empty()) {
    if (plain_only) {
      throw smtp_exception("refusing to send AUTH credentials in clear; set security=starttls or security=tls");
    }
    do_auth(conn, cfg.username, cfg.password, ehlo_caps);
  }

  // MAIL FROM
  conn.write("MAIL FROM:<" + msg.from + ">\r\n");
  r = conn.read_reply();
  expect_status(r, '2', "MAIL FROM");

  // RCPT TO
  conn.write("RCPT TO:<" + msg.to + ">\r\n");
  r = conn.read_reply();
  expect_status(r, '2', "RCPT TO");

  // DATA
  conn.write("DATA\r\n");
  r = conn.read_reply();
  if (!reply_starts_with(r, "354")) throw smtp_exception("DATA rejected: " + r);

  // Construct headers + body. The body is dot-stuffed / CRLF-normalised.
  std::ostringstream headers;
  headers << "From: " << msg.from << "\r\n";
  headers << "To: " << msg.to << "\r\n";
  if (!subject.empty()) headers << "Subject: " << subject << "\r\n";
  headers << "MIME-Version: 1.0\r\n";
  headers << "Content-Type: text/plain; charset=utf-8\r\n";
  // Date header is RFC-required for many strict relays (Outlook, Gmail).
  {
    const auto now = boost::posix_time::second_clock::universal_time();
    // RFC 5322 date format. boost's date_facet is locale-dependent; we
    // build the string manually for predictability.
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char* dows[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d +0000", dows[now.date().day_of_week()], static_cast<int>(now.date().day().as_number()),
                  months[now.date().month().as_number() - 1], static_cast<int>(now.date().year()), static_cast<int>(now.time_of_day().hours()),
                  static_cast<int>(now.time_of_day().minutes()), static_cast<int>(now.time_of_day().seconds()));
    headers << "Date: " << buf << "\r\n";
  }
  headers << "\r\n";

  conn.write(headers.str() + detail::dot_stuff_and_crlf(msg.body) + ".\r\n");
  r = conn.read_reply();
  expect_status(r, '2', "end of DATA");

  // QUIT (best-effort - if the server has already closed we don't care).
  try {
    conn.write("QUIT\r\n");
    (void)conn.read_reply();
  } catch (const smtp_exception&) {
    // ignore
  }
}

}  // namespace smtp
