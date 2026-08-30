// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "smtp.hpp"

#include <openssl/rand.h>

#include <algorithm>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <bytes/base64.hpp>
#include <chrono>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace smtp {

// Defined ahead of the connection code because every error path below that
// quotes server text routes it through here first.
namespace detail {

// Reply bytes are the peer's to choose, and exception messages carry them
// into the agent log (NSC_LOG_ERROR) and the submit response. Left raw, a
// reply can embed terminal escape sequences for whoever tails the log, and
// the '\n' that read_reply() joins a multi-line reply with lets one reply
// masquerade as several log lines. Render it inert instead: the line join
// becomes " / ", anything that is not printable US-ASCII becomes '?', and
// the result is truncated.
//
// Printable ASCII is the whole allowlist rather than "everything except the
// C0 controls", because a byte does not have to be C0 to steer a terminal.
// The C1 range (0x80-0x9F) carries single-byte equivalents of the escape
// sequences - 0x9B is CSI - which a terminal in a non-UTF-8 locale acts on,
// and their UTF-8 spellings (0xC2 0x80..0x9F) reach the same code points in
// a terminal that does decode UTF-8. Allowlisting covers both without
// enumerating either, and it cannot emit the half-scrubbed byte sequences
// that dropping individual bytes out of a multi-byte character would. The
// cost is that legitimately non-ASCII reply text (RFC 6531 permits it)
// flattens to '?' - acceptable for a diagnostic string whose reason for
// existing is to be safe to display.
std::string scrub_reply(const std::string& reply) {
  constexpr std::size_t max_len = 500;
  std::string out;
  out.reserve(std::min(reply.size(), max_len) + 4);
  for (const char c : reply) {
    if (out.size() >= max_len) {
      out += "...";
      break;
    }
    const auto u = static_cast<unsigned char>(c);
    if (c == '\n') {
      out += " / ";
    } else if (u < 0x20 || u >= 0x7f) {
      out.push_back('?');
    } else {
      out.push_back(c);
    }
  }
  return out;
}

}  // namespace detail

namespace {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using ssl_stream = asio::ssl::stream<tcp::socket>;

// The timeout bounds how long a submission may take; these bound how much a
// peer can make it buffer. Without them read_until() accumulates until the
// deadline - and the deadline is seconds, so a server (or a MITM on a
// security=none target) that streams bytes without ever sending CRLF turns
// the budget into gigabytes of agent memory at line rate. RFC 5321 4.5.3.1
// caps a reply line at 512 bytes and no real server sends EHLO replies
// hundreds of lines long, so both limits are far above anything legitimate.
constexpr std::size_t max_reply_line_bytes = 64 * 1024;
constexpr std::size_t max_reply_lines = 100;

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
  // `timeout_seconds` is the budget for the whole submission, not for each
  // individual operation: an SMTP session is a fixed handful of round trips,
  // and an operator setting timeout=30 means "give up on this notification
  // after 30 seconds", not "allow 30 seconds per read". Per-operation
  // deadlines multiplied out to minutes of worst case - a slow peer that
  // answers just inside the deadline every time, or a host name resolving to
  // several dead addresses, each of which got a fresh deadline of its own.
  sync_io(asio::io_context& io, tcp::socket& s, int timeout_seconds)
      : io_(io),
        socket_ref_(s),
        tls_stream_(nullptr),
        timeout_seconds_(timeout_seconds),
        deadline_(std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds)) {}

  // Render a failure for an operator. A spent budget is ours to explain, not
  // the platform's: asio maps the deadline to ETIMEDOUT / WSAETIMEDOUT, whose
  // system text describes a peer that did not answer - on Windows, "a
  // connection attempt failed because the connected party did not properly
  // respond". That blames the server for a deadline this client set, and it
  // reads differently on every platform. Say what actually happened instead.
  std::string describe(const boost::system::error_code& ec) const {
    if (ec == asio::error::timed_out) {
      return "timed out after " + std::to_string(timeout_seconds_) + "s (the budget for the whole submission)";
    }
    return ec.message();
  }

  // Run the TLS handshake under the same deadline as every other operation,
  // then route subsequent reads and writes through the encrypted stream.
  // asio's synchronous handshake() takes no timeout, so a peer that stalls
  // part-way through one would hang the submission thread indefinitely - the
  // very thing run_with_deadline() exists to prevent everywhere else.
  void handshake(ssl_stream& s, const char* what) {
    // Nothing may be left in the receive buffer when we hand the socket to
    // TLS. read_until() reads whole segments, so a server (or a man in the
    // middle who can inject one packet) that appends bytes to the STARTTLS
    // greeting leaves them sitting here in cleartext - and every read after
    // the handshake would consume them as if they had arrived authenticated
    // inside the TLS session. That is the STARTTLS command/response injection
    // family (CVE-2011-0411 and relatives): forged capabilities in the
    // re-EHLO, or forged 2xx acknowledgements that make the agent report a
    // notification as delivered when nothing was ever sent. RFC 3207 4
    // requires the client to discard any knowledge obtained before the
    // handshake, so refuse the session rather than trust the bytes.
    if (buf_.size() > 0) {
      throw smtp_exception(std::string(what) + " aborted: server sent " + std::to_string(buf_.size()) +
                           " byte(s) of unexpected data before the handshake (possible STARTTLS response injection)");
    }
    boost::system::error_code ec;
    run_with_deadline(
        [&](auto&& done) { s.async_handshake(asio::ssl::stream_base::client, [done = std::move(done)](const boost::system::error_code& e) { done(e); }); }, ec);
    if (ec) throw smtp_exception(std::string(what) + " failed: " + describe(ec));
    tls_stream_ = &s;
  }

  // Resolve under the shared budget. The platform resolver's own timeouts are
  // long (and not ours to set), so a black-holed name server would otherwise
  // stall the submission well past the configured timeout before any socket
  // operation got a chance to be deadlined.
  tcp::resolver::results_type resolve(const std::string& host, const std::string& port) {
    tcp::resolver resolver(io_);
    tcp::resolver::results_type endpoints;
    boost::system::error_code ec;
    run_with_deadline(
        [&](auto&& done) {
          resolver.async_resolve(host, port, [&endpoints, done = std::move(done)](const boost::system::error_code& e, tcp::resolver::results_type r) {
            endpoints = std::move(r);
            done(e);
          });
        },
        ec);
    if (ec) throw smtp_exception("DNS resolve failed: " + describe(ec));
    return endpoints;
  }

  // Connect to the first endpoint that succeeds, with a deadline.
  void connect(const tcp::resolver::results_type& endpoints) {
    boost::system::error_code ec = asio::error::host_not_found;
    for (auto it = endpoints.begin(); ec && it != endpoints.end(); ++it) {
      // Non-throwing close: an incidental failure here (closing a socket that
      // was never opened, on the first pass) would otherwise escape as a
      // boost::system::system_error rather than the smtp_exception every
      // caller of send() catches.
      boost::system::error_code close_ec;
      socket_ref_.close(close_ec);
      const auto ep = it->endpoint();
      run_with_deadline([&](auto&& done) { socket_ref_.async_connect(ep, [done = std::move(done)](const boost::system::error_code& e) { done(e); }); }, ec);
      // A spent budget ends the walk, not just this attempt: the deadline
      // covers the whole submission, so every remaining endpoint would time
      // out the same way before its connect could complete.
      if (ec == asio::error::timed_out) break;
    }
    if (ec) throw smtp_exception("connect failed: " + describe(ec));
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
    if (ec) throw smtp_exception("write failed: " + describe(ec));
  }

  // Read one CRLF-terminated SMTP reply. Multi-line replies are returned with
  // their lines joined by '\n' (the caller gets every line up to and including
  // the final "NNN <space>"). The separator matters: capability lookups run
  // per line, and running the lines together also made error messages
  // unreadable.
  std::string read_reply() {
    std::string out;
    std::size_t lines = 0;
    while (true) {
      if (++lines > max_reply_lines) {
        throw smtp_exception("SMTP reply exceeds " + std::to_string(max_reply_lines) + " lines; giving up");
      }
      const std::string line = read_line();
      if (!out.empty()) out.push_back('\n');
      out += line;
      // RFC 5321 4.2: each reply line is "NNN-text" for continuation,
      // "NNN text" for the final line. A line shorter than 4 bytes is a
      // protocol error.
      if (line.size() < 4) {
        throw smtp_exception("malformed SMTP reply: '" + detail::scrub_reply(line) + "'");
      }
      if (line[3] == ' ') break;
      if (line[3] != '-') throw smtp_exception("malformed SMTP reply: '" + detail::scrub_reply(line) + "'");
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
    // not_found is read_until() reporting that buf_ hit its size cap without
    // a CRLF anywhere in it - a peer streaming an endless line, not a socket
    // problem, so name it as the protocol violation it is.
    if (ec == asio::error::not_found) {
      throw smtp_exception("SMTP reply line exceeds " + std::to_string(max_reply_line_bytes) + " bytes; giving up");
    }
    if (ec) throw smtp_exception("read failed: " + describe(ec));
    std::istream is(&buf_);
    std::string line;
    std::getline(is, line);  // strips '\n'
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
  }

  // Run a single async op until it completes or the deadline expires. Sets
  // `out_ec` to the operation's result, or timed_out on timeout.
  //
  // The op's completion handler can outlive this frame. On timeout the timer
  // handler stop()s the io_context, and the cancelled op's handler - already
  // queued, or queued by the cancel - is left unexecuted; it runs on the NEXT
  // restart()/run(), which happens when a caller issues another operation
  // after a timeout (connect() walking its endpoint list). A handler that
  // captured this frame's locals by reference would then write through
  // dangling references into whatever occupies the stack now. So everything
  // the handler touches lives in `op`, co-owned by the handler itself: a
  // stale handler completes against its own orphaned state, which nobody
  // reads, and the current operation's state is untouchable by it.
  //
  // The timer handler needs no such care: run() drains it before returning
  // in every case (it is either what called stop(), or it completes as
  // aborted before run() runs out of work), so its frame is always alive.
  template <typename Init>
  void run_with_deadline(Init&& init, boost::system::error_code& out_ec, std::size_t* out_bytes = nullptr) {
    struct op_state {
      explicit op_state(asio::io_context& io) : timer(io), ec(asio::error::would_block), bytes(0), timed_out(false) {}
      asio::steady_timer timer;
      boost::system::error_code ec;
      std::size_t bytes;
      bool timed_out;
    };
    const auto op = std::make_shared<op_state>(io_);
    // A deadline already in the past fires immediately, which is what we want
    // once the budget is spent.
    op->timer.expires_at(deadline_);
    op->timer.async_wait([this, op](const boost::system::error_code& e) {
      if (e == asio::error::operation_aborted) return;
      op->timed_out = true;
      // Cancel any outstanding I/O so run() returns. stop() covers the
      // operations cancelling the socket does not reach - a resolve in
      // particular runs off on its own thread and would otherwise keep run()
      // going until the platform resolver gave up on its own schedule.
      boost::system::error_code ignore;
      socket_ref_.cancel(ignore);
      io_.stop();
    });
    init([op](const boost::system::error_code& e, std::size_t n = 0) {
      op->ec = e;
      op->bytes = n;
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow it so an incidental failure
      // can't escape this handler and misreport a successful operation.
      try {
        op->timer.cancel();
      } catch (...) {
      }
    });
    io_.restart();
    io_.run();
    out_ec = op->timed_out ? asio::error::timed_out : op->ec;
    if (out_bytes) *out_bytes = op->bytes;
  }

  asio::io_context& io_;
  tcp::socket& socket_ref_;
  ssl_stream* tls_stream_;
  // Capped so read_until() fails with not_found instead of buffering without
  // bound; the leftover-data check in handshake() counts against it too.
  asio::streambuf buf_{max_reply_line_bytes};
  int timeout_seconds_;
  std::chrono::steady_clock::time_point deadline_;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void expect_status(const std::string& reply, char expected_first, const std::string& context) {
  if (reply.size() < 3 || reply[0] != expected_first) {
    throw smtp_exception(context + " unexpected reply: " + detail::scrub_reply(reply));
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

// The EHLO argument is interpolated straight into a command line, so it needs
// the same treatment the envelope addresses get. It is not always operator
// data: it defaults to the submitting sender's host name, which for a relayed
// submission arrives in the request header from whoever sent it. A CR / LF
// there would end the EHLO and start a command of the attacker's choosing on
// an authenticated submission session - their MAIL FROM and RCPT TO, sent as
// us. A space is refused for the same reason at a smaller scale: it would let
// them append EHLO parameters.
//
// RFC 5321 4.1.1.1 allows a domain name or an address literal here, so the
// permitted set is letters, digits, and the punctuation those two forms need.
void validate_ehlo_name(const std::string& name) {
  if (name.empty()) {
    throw smtp_exception("EHLO name is empty");
  }
  for (const char c : name) {
    const auto u = static_cast<unsigned char>(c);
    const bool alnum = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9');
    const bool punct = (c == '.' || c == '-' || c == '_' || c == '[' || c == ']' || c == ':');
    if (!alnum && !punct) {
      throw smtp_exception("EHLO name contains an illegal character; expected a host name or an address literal");
    }
  }
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

namespace {

// The capability a reply line advertises: the first token after the 4-byte
// "250-" / "250 " reply code, uppercased. Empty when the line carries no
// capability (the greeting line, or anything too short to hold a code).
std::string capability_of(const std::string& line) {
  if (line.size() < 4) return std::string();
  std::size_t begin = 4;
  while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t')) ++begin;
  std::size_t end = begin;
  while (end < line.size() && line[end] != ' ' && line[end] != '\t') ++end;
  return boost::algorithm::to_upper_copy(line.substr(begin, end - begin));
}

// The parameters following the capability on a reply line, split on spaces
// and uppercased. For "250 AUTH PLAIN LOGIN" this is {"PLAIN", "LOGIN"}.
std::vector<std::string> capability_params(const std::string& line) {
  std::vector<std::string> out;
  std::istringstream is(line.size() < 4 ? std::string() : line.substr(4));
  std::string token;
  bool first = true;
  while (is >> token) {
    if (first) {
      // The capability name itself.
      first = false;
      continue;
    }
    out.push_back(boost::algorithm::to_upper_copy(token));
  }
  return out;
}

std::vector<std::string> reply_lines(const std::string& reply) {
  std::vector<std::string> lines;
  boost::algorithm::split(lines, reply, boost::algorithm::is_any_of("\n"));
  return lines;
}

}  // namespace

// A capability is the first token of its own reply line, not a substring of
// the reply as a whole. Matching the whole blob let a server's free text - a
// greeting naming the software, an unrelated capability's parameter, or two
// lines run together at the join - answer for a capability the server never
// advertised. That mattered most for STARTTLS: "did the server offer
// STARTTLS" is what decides whether the session gets encrypted at all.
bool has_capability(const std::string& ehlo_reply, const std::string& keyword) {
  const std::string wanted = boost::algorithm::to_upper_copy(keyword);
  for (const std::string& line : reply_lines(ehlo_reply)) {
    if (capability_of(line) == wanted) return true;
  }
  return false;
}

// Mail without a Message-ID is treated as suspicious by essentially every
// spam filter, and it is the identifier a mail admin traces a lost
// notification by, so a monitoring agent that omits it is exactly the wrong
// place to save two lines. Uniqueness is all that is required of the local
// part - nothing here is a security decision - but OpenSSL is already linked
// so its CSPRNG is the cheapest source; a clock and a counter cover the case
// where it refuses.
std::string make_message_id(const std::string& from, const std::string& ehlo_name) {
  // Right hand side: the sender's own domain, so the id lines up with the
  // address the message claims to be from.
  std::string domain;
  const std::size_t at = from.rfind('@');
  if (at != std::string::npos && at + 1 < from.size()) domain = from.substr(at + 1);
  if (domain.empty()) domain = ehlo_name;

  // The domain lands in a header, so hold it to the same character set the
  // EHLO name is held to rather than trusting where it came from.
  std::string safe_domain;
  for (const char c : domain) {
    const auto u = static_cast<unsigned char>(c);
    const bool alnum = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9');
    if (alnum || c == '.' || c == '-' || c == '_') safe_domain.push_back(c);
  }
  if (safe_domain.empty()) safe_domain = "localhost";

  static const char* kHex = "0123456789abcdef";
  std::string unique;
  unsigned char raw[16];
  if (RAND_bytes(raw, static_cast<int>(sizeof(raw))) == 1) {
    for (const unsigned char b : raw) {
      unique.push_back(kHex[b >> 4]);
      unique.push_back(kHex[b & 0x0f]);
    }
  } else {
    static std::atomic<unsigned long long> counter(0);
    std::ostringstream fallback;
    fallback << std::hex << std::chrono::system_clock::now().time_since_epoch().count() << '.' << ++counter;
    unique = fallback.str();
  }
  return "<" + unique + "@" + safe_domain + ">";
}

std::vector<std::string> auth_mechanisms(const std::string& ehlo_reply) {
  for (const std::string& line : reply_lines(ehlo_reply)) {
    if (capability_of(line) == "AUTH") return capability_params(line);
  }
  return std::vector<std::string>();
}

}  // namespace detail

namespace {  // re-open anon namespace for the rest of the helpers

std::string b64(const std::string& s) { return bytes::base64_encode(s); }

void do_auth(sync_io& io, const std::string& user, const std::string& pass, const std::string& ehlo_response) {
  // Prefer AUTH PLAIN when the server advertises it (most servers do).
  // Fall back to AUTH LOGIN otherwise. Both are TLS-only here because we
  // refuse to enter this function over an unencrypted channel - see
  // send().
  const std::vector<std::string> mechanisms = detail::auth_mechanisms(ehlo_response);
  const auto advertises = [&mechanisms](const char* m) { return std::find(mechanisms.begin(), mechanisms.end(), m) != mechanisms.end(); };

  if (mechanisms.empty()) {
    throw smtp_exception("a username is configured but the server does not advertise AUTH: " + detail::scrub_reply(ehlo_response));
  }
  if (!advertises("PLAIN") && !advertises("LOGIN")) {
    throw smtp_exception("server advertises no AUTH mechanism we support (offered: " + detail::scrub_reply(boost::algorithm::join(mechanisms, ", ")) +
                         "; supported: PLAIN, LOGIN)");
  }

  if (advertises("PLAIN")) {
    // RFC 4616: "\0username\0password" base64-encoded.
    std::string sasl;
    sasl.push_back('\0');
    sasl += user;
    sasl.push_back('\0');
    sasl += pass;
    io.write("AUTH PLAIN " + b64(sasl) + "\r\n");
    const std::string r = io.read_reply();
    if (!reply_starts_with(r, "235")) throw smtp_exception("AUTH PLAIN rejected: " + detail::scrub_reply(r));
    return;
  }
  io.write("AUTH LOGIN\r\n");
  std::string r = io.read_reply();
  if (!reply_starts_with(r, "334")) throw smtp_exception("AUTH LOGIN not accepted: " + detail::scrub_reply(r));
  io.write(b64(user) + "\r\n");
  r = io.read_reply();
  if (!reply_starts_with(r, "334")) throw smtp_exception("AUTH LOGIN username rejected: " + detail::scrub_reply(r));
  io.write(b64(pass) + "\r\n");
  r = io.read_reply();
  if (!reply_starts_with(r, "235")) throw smtp_exception("AUTH LOGIN password rejected: " + detail::scrub_reply(r));
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

  // RFC 4616 frames AUTH PLAIN as NUL-separated fields, so a NUL inside the
  // username would shift the authcid/password boundary and authenticate as
  // someone else's credential split. Config strings should never carry one;
  // refuse rather than reinterpret if one does.
  if (cfg.username.find('\0') != std::string::npos || cfg.password.find('\0') != std::string::npos) {
    throw smtp_exception("AUTH username/password must not contain NUL bytes");
  }

  // Validated up here with the addresses, before anything is put on the wire.
  const std::string ehlo_name = cfg.canonical_name.empty() ? std::string("localhost") : cfg.canonical_name;
  detail::validate_ehlo_name(ehlo_name);

  // Validate security mode early so a typo does not silently fall through
  // to plain transport.
  const std::string sec = boost::algorithm::to_lower_copy(cfg.security);
  const bool tls_immediate = (sec == "tls" || sec == "ssl");
  const bool starttls = (sec == "starttls");
  const bool plain_only = (sec == "none");
  if (!tls_immediate && !starttls && !plain_only) {
    throw smtp_exception("invalid security mode '" + cfg.security + "' (expected none|starttls|tls, or ssl as an alias for tls)");
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
    // Verify against the bundle the agent was configured with. This defaults
    // to ${ca-path}, which on unix is the distribution's own bundle and on
    // Windows is the ROOT store the service exports at boot - OpenSSL's
    // default verify paths do not include the Windows certificate store at
    // all, so relying on them alone made verify_peer fail against every public
    // provider there and left insecure-skip-verify as the only way through.
    // Loading is only attempted when verification is on, so a missing bundle
    // cannot break a deliberately unverified session.
    if (!cfg.ca_path.empty() && cfg.ca_path != "none") {
      boost::system::error_code ec;
      ssl_ctx.load_verify_file(cfg.ca_path, ec);
      if (ec) throw smtp_exception("failed to load CA bundle '" + cfg.ca_path + "': " + ec.message());
    } else {
      ssl_ctx.set_default_verify_paths();
    }
  }

  asio::io_context io;
  // The owning ssl_stream<tcp::socket> form keeps the underlying socket
  // accessible via next_layer(). We do plain SMTP reads/writes against
  // next_layer() before STARTTLS, then handshake() upgrades subsequent IO
  // to flow through `tls`.
  ssl_stream tls(io, ssl_ctx);
  if (!cfg.insecure_skip_verify) {
    tls.set_verify_callback(asio::ssl::host_name_verification(cfg.server));
  }
  // SNI: a server hosting several certs needs the server name to return the
  // right one; without it it may answer with its default cert and fail the
  // hostname check above. Set once here for both the immediate-TLS and STARTTLS
  // handshakes below. Mirrors the HTTP client.
  if (!cfg.server.empty()) {
    SSL_set_tlsext_host_name(tls.native_handle(), cfg.server.c_str());
  }

  sync_io conn(io, tls.next_layer(), cfg.timeout_seconds);

  // Resolve and connect.
  conn.connect(conn.resolve(cfg.server, cfg.port));

  if (tls_immediate) {
    conn.handshake(tls, "TLS handshake");
  }

  // Banner.
  std::string r = conn.read_reply();
  expect_status(r, '2', "banner");

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
    if (!detail::has_capability(ehlo_caps, "STARTTLS")) {
      throw smtp_exception("server did not advertise STARTTLS but security=starttls was requested");
    }
    conn.write("STARTTLS\r\n");
    r = conn.read_reply();
    // Exactly 220, per RFC 3207 4: any other 2xx here is not "ready to start
    // TLS", and proceeding to handshake against it helps nobody.
    if (!reply_starts_with(r, "220")) throw smtp_exception("STARTTLS unexpected reply: " + detail::scrub_reply(r));
    conn.handshake(tls, "TLS handshake (STARTTLS)");
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
  if (!reply_starts_with(r, "354")) throw smtp_exception("DATA rejected: " + detail::scrub_reply(r));

  // Construct headers + body. The body is dot-stuffed / CRLF-normalised.
  std::ostringstream headers;
  headers << "From: " << msg.from << "\r\n";
  headers << "To: " << msg.to << "\r\n";
  if (!subject.empty()) headers << "Subject: " << subject << "\r\n";
  headers << "MIME-Version: 1.0\r\n";
  headers << "Content-Type: text/plain; charset=utf-8\r\n";
  headers << "Message-ID: " << detail::make_message_id(msg.from, ehlo_name) << "\r\n";
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
