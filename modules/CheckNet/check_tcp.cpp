// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_tcp.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#endif
#include <boost/chrono.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <memory>
#include <vector>
#include <net/address_family.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include "check_net_error.hpp"
#include "check_starttls.hpp"

namespace po = boost::program_options;

namespace check_net {
namespace check_tcp_filter {

filter_obj_handler::filter_obj_handler() {
  register_common_keywords(registry_);

  // TLS certificate of the connected peer. Registered here rather than in the
  // common set because check_ssh is never TLS and would only advertise a
  // keyword that can never be populated.
  registry_
      .add_optional_int_var("ssl_expiry_days", [](auto obj) { return obj->get_ssl_expiry_days_opt(); }, "no certificate",
                            "Whole days until the peer's TLS certificate expires; negative once it has expired. Renders as 'no certificate' (and compares "
                            "false against every number) when the connection is not TLS, the peer presented none, or the certificate's notAfter could not "
                            "be read - so `ssl_expiry_days < 30` cannot fire on any of them; `ssl_expiry_days = 'no certificate'` tests for that state, and "
                            "has_certificate tells a served-but-unreadable certificate apart from no certificate at all.")
      .add_int_perf("", "", "_ssl_expiry_days");
  registry_
      .add_int_var("has_certificate", parsers::where::type_int, &filter_obj::get_has_certificate,
                   "1 when the peer presented a TLS certificate, 0 otherwise")
      .no_perf();
  cert::register_keywords<filter_obj>(registry_, &filter_obj::cert);
}

}  // namespace check_tcp_filter

namespace check_ssh_filter {

filter_obj_handler::filter_obj_handler() {
  check_tcp_filter::register_common_keywords(registry_);

  // Fields parsed out of the SSH identification string. They are empty (and the
  // numeric ones 0) whenever no banner was read — a refused/timed-out
  // connection, or a port that is not speaking SSH — so guard on `result` when
  // that distinction matters.
  registry_.add_string_var("banner", &filter_obj::get_banner, "The raw SSH identification string, e.g. SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5");
  registry_.add_string_var("protocol", &filter_obj::get_protocol, "SSH protocol version the server announced, e.g. 2.0 or 1.99");
  registry_.add_string_var("version", &filter_obj::get_version, "Software version the server announced, e.g. OpenSSH_9.6p1");
  registry_.add_string_var("software", &filter_obj::get_software, "Software name from the version string, e.g. OpenSSH or dropbear");
  registry_.add_string_var("software_version", &filter_obj::get_software_version, "Software version number from the version string, e.g. 9.6p1 or 2022.83");
  registry_.add_string_var("comments", &filter_obj::get_comments, "Trailing comments of the identification string, e.g. the distribution patch level");
  registry_.add_int_var("protocol_major", parsers::where::type_int, &filter_obj::get_protocol_major,
                        "Major SSH protocol version as a number (2 for 2.0); use protocol_major < 2 to catch an SSHv1-only server")
      .no_perf();
  registry_
      .add_int_var("protocol_minor", parsers::where::type_int, &filter_obj::get_protocol_minor,
                   "Minor SSH protocol version as a number (0 for 2.0, 99 for 1.99)")
      .no_perf();
}

}  // namespace check_ssh_filter

namespace {

using boost::asio::ip::tcp;

using steady_clock = std::chrono::steady_clock;

// The shape every timed operation in this file shares: arm a timer that closes
// the socket once the deadline passes, start one asynchronous operation, run
// the io_context until whichever of the two finishes first, and report which
// it was. Returns true when the deadline was what ended it.
//
// Closing the socket is what actually unblocks an operation that overran -
// asio has no "give up on this read after N ms" - and it makes that operation
// fail with operation_aborted, which is indistinguishable from the peer having
// reset the connection. So the timer records that it fired, and that flag,
// rather than the operation's error code, is what a caller reads to tell a
// timeout from a disconnect.
//
// `start` is handed a `finished` callable to invoke from its own completion
// handler; it lives in this frame and stays alive for the whole run().
template <typename Start>
bool run_with_deadline(tcp::socket &socket, boost::asio::io_context &io_service, const steady_clock::time_point deadline, Start start) {
  bool done = false;
  bool deadline_hit = false;
  boost::asio::steady_timer timer(io_service);

  timer.expires_at(deadline);
  timer.async_wait([&](const boost::system::error_code &ec) {
    if (!ec && !done) {
      deadline_hit = true;
      boost::system::error_code ignore;
      socket.close(ignore);
    }
  });

  const auto finished = [&]() {
    done = true;
    // cancel() can throw (the non-throwing cancel(ec) overload is removed
    // under BOOST_ASIO_NO_DEPRECATED). Swallow it so an incidental failure
    // cannot escape an asio handler and misreport an operation that worked.
    try {
      timer.cancel();
    } catch (...) {
    }
  };
  start(finished);

  io_service.run();
  io_service.restart();
  return deadline_hit;
}

// Send the optional payload then read the peer's response (with a deadline) and
// apply the expect / expect_regex matchers. Works over any stream that supports
// boost::asio::async_write / async_read (a plain tcp::socket or a TLS stream).
// `lowest` is the underlying socket, closed to unblock an operation that
// overruns the timeout.
template <typename Stream>
void tcp_converse(Stream &stream, tcp::socket &lowest, boost::asio::io_context &io_service, int timeout_ms, const std::string &send_data,
                  const std::string &expect, const std::string &expect_regex, check_tcp_filter::filter_obj &out) {
  if (!send_data.empty()) {
    // Deadlined like the read: a write blocks until the peer's receive window
    // opens, so a peer that connects and then never reads would otherwise hold
    // the check open indefinitely. Payloads here are small, but how small a
    // write has to be to complete is a property of the receiver.
    boost::system::error_code write_ec = boost::asio::error::would_block;
    const bool timed_out = run_with_deadline(lowest, io_service, steady_clock::now() + std::chrono::milliseconds(timeout_ms), [&](const auto &finished) {
      boost::asio::async_write(stream, boost::asio::buffer(send_data), [&](const boost::system::error_code &ec, std::size_t) {
        write_ec = ec;
        finished();
      });
    });
    if (timed_out) {
      out.result = "write_timeout";
      return;
    }
    if (write_ec) {
      out.result = "write_failed";
      return;
    }
  }

  if (expect.empty() && expect_regex.empty()) return;

  // Read whatever the peer sends within a small window, with a deadline.
  boost::asio::streambuf response_buf;
  boost::system::error_code read_ec = boost::asio::error::would_block;
  const bool timed_out = run_with_deadline(lowest, io_service, steady_clock::now() + std::chrono::milliseconds(timeout_ms), [&](const auto &finished) {
    boost::asio::async_read(stream, response_buf, boost::asio::transfer_at_least(1), [&](const boost::system::error_code &ec, std::size_t) {
      read_ec = ec;
      finished();
    });
  });

  // A peer that went quiet is not a peer that hung up, and the deadline is the
  // only thing that tells them apart: closing the socket to unblock the read
  // makes it fail with operation_aborted either way.
  if (timed_out) {
    out.result = "read_timeout";
    return;
  }

  // A peer that closes cleanly reports eof; a TLS peer that closes without a
  // close_notify reports stream_truncated. Both mean "no more data" — evaluate
  // whatever we received rather than failing the check.
  bool clean_end = (read_ec == boost::asio::error::eof);
#ifdef USE_SSL
  if (read_ec == boost::asio::ssl::error::stream_truncated) clean_end = true;
#endif
  if (read_ec && !clean_end) {
    out.result = "read_failed";
    return;
  }

  const std::string data{boost::asio::buffers_begin(response_buf.data()), boost::asio::buffers_end(response_buf.data())};
  out.response = boost::trim_copy(data);

  bool matched = true;
  if (!expect.empty() && data.find(expect) == std::string::npos) matched = false;
  if (matched && !expect_regex.empty()) {
    try {
      if (!boost::regex_search(data, boost::regex(expect_regex))) matched = false;
    } catch (const std::exception &) {
      out.result = "error: invalid expect regex";
      return;
    }
  }
  out.result = matched ? "ok" : "no_match";
}

// Everything that steers one connection attempt. Kept in one struct so the
// growing TLS/STARTTLS set does not turn into a dozen positional parameters.
struct tcp_check_options {
  int timeout_ms = 5000;
  std::string send_data;
  std::string expect;
  std::string expect_regex;
  bool use_tls = false;
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode = "none";
  std::string ca_file;
  // The TLS name: sent as SNI, and what the certificate is verified against.
  // Empty means the host we dialed.
  std::string sni;
  // Opportunistic TLS, negotiated in the clear before the handshake.
  const starttls::preset *starttls_preset = nullptr;
  std::vector<std::string> required_sans;
  net::address_family af = net::address_family::any;
};

// How much a STARTTLS negotiation may buffer before we give up on it. A
// greeting and a handful of capability lines are a few hundred bytes; a peer
// that streams more than this without ever answering is not negotiating, and
// the deadline alone would let it push megabytes into an agent's memory first.
//
// What this actually bounds is UNCONSUMED bytes. The line engine takes each
// complete line out of the buffer as it arrives, so a peer chattering endless
// complete lines is held by the deadline instead - it is answering, just never
// with the right thing. The cap is for a reply that never ends: no newline, so
// nothing can be drained, and every byte received stays.
const std::size_t max_negotiation_bytes = 64 * 1024;

// Why a read stopped. A peer that hangs up is not a peer that went quiet, and
// collapsing the two would describe an instant disconnect as a timeout - the
// slowest-sounding answer for the fastest failure there is. A peer that floods
// is a third thing again: calling that a timeout sends the operator to raise
// `timeout=`, which cannot help, because the budget that ran out was bytes.
enum class read_outcome {
  data,          // something arrived and was appended
  timed_out,     // the deadline passed with no answer
  overflowed,    // the negotiation outgrew max_negotiation_bytes
  disconnected,  // the peer closed, reset, or the socket failed
};

// Append whatever arrives on the socket to `buffer`, bounded by `deadline` and
// by max_negotiation_bytes.
read_outcome read_some_until(tcp::socket &socket, boost::asio::io_context &io_service, const steady_clock::time_point deadline, std::string &buffer) {
  if (steady_clock::now() >= deadline) return read_outcome::timed_out;
  if (buffer.size() >= max_negotiation_bytes) return read_outcome::overflowed;
  char chunk[1024];
  boost::system::error_code read_ec = boost::asio::error::would_block;
  std::size_t received = 0;

  const bool timed_out = run_with_deadline(socket, io_service, deadline, [&](const auto &finished) {
    socket.async_read_some(boost::asio::buffer(chunk, sizeof(chunk)), [&](const boost::system::error_code &ec, const std::size_t transferred) {
      read_ec = ec;
      received = transferred;
      finished();
    });
  });

  if (timed_out) return read_outcome::timed_out;
  if (read_ec) return read_outcome::disconnected;
  buffer.append(chunk, received);
  return read_outcome::data;
}

// Write under the negotiation's own deadline. A synchronous write blocks until
// the peer's receive window opens, so a peer that connects and then never reads
// could hold the check open past timeout_ms however small the payload is - how
// small a write has to be to complete is a property of the receiver, not of us.
bool write_all(tcp::socket &socket, boost::asio::io_context &io_service, const steady_clock::time_point deadline, const std::string &data) {
  if (data.empty()) return true;
  boost::system::error_code write_ec = boost::asio::error::would_block;
  const bool timed_out = run_with_deadline(socket, io_service, deadline, [&](const auto &finished) {
    boost::asio::async_write(socket, boost::asio::buffer(data), [&](const boost::system::error_code &ec, std::size_t) {
      write_ec = ec;
      finished();
    });
  });
  return !timed_out && !write_ec;
}

// How one step of a negotiation ended. Distinct from starttls::verdict because
// "no answer" splits into two outcomes the operator needs told apart.
enum class step_result { matched, refused, timed_out, overflowed, disconnected };

// Map a read that produced no answer onto the step that was waiting for one.
step_result step_from_read(const read_outcome outcome) {
  if (outcome == read_outcome::disconnected) return step_result::disconnected;
  if (outcome == read_outcome::overflowed) return step_result::overflowed;
  return step_result::timed_out;
}

// Read lines until one answers the step being awaited.
step_result await_line(tcp::socket &socket, boost::asio::io_context &io_service, const steady_clock::time_point deadline, std::string &buffer,
                       const boost::regex &expect, const boost::regex &failure) {
  for (;;) {
    for (const std::string &line : starttls::take_complete_lines(buffer)) {
      const starttls::verdict answered = starttls::classify_line(line, expect, failure);
      if (answered == starttls::verdict::matched) return step_result::matched;
      if (answered == starttls::verdict::failed) return step_result::refused;
    }
    const read_outcome outcome = read_some_until(socket, io_service, deadline, buffer);
    if (outcome != read_outcome::data) return step_from_read(outcome);
  }
}

// The plaintext half of an opportunistic-TLS upgrade: true when the server
// agreed and the socket is ready for a handshake. On false `out.result` says
// why, in the same short-status-word vocabulary as the rest of the check.
bool negotiate_starttls(tcp::socket &socket, boost::asio::io_context &io_service, const int timeout_ms, const starttls::preset &preset,
                        check_tcp_filter::filter_obj &out) {
  // One deadline for the whole negotiation rather than one per read: a server
  // that trickles out a line at a time must not be able to extend it
  // indefinitely.
  const steady_clock::time_point deadline = steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  // Compiled once per check rather than once per received line: the patterns
  // are fixed table data, and a chatty peer can push thousands of lines at the
  // classifier before the budget runs out.
  const starttls::compiled_preset patterns = starttls::compile(preset);
  std::string buffer;

  const auto refuse = [&out](const char *reason) {
    out.result = reason;
    return false;
  };
  const auto failed = [&refuse](const step_result result) {
    if (result == step_result::refused) return refuse("starttls_refused");
    if (result == step_result::disconnected) return refuse("starttls_disconnected");
    // Distinct from the timeout: the budget that ran out was bytes, not
    // milliseconds, so raising `timeout=` would change nothing.
    if (result == step_result::overflowed) return refuse("starttls_overflow");
    return refuse("starttls_timeout");
  };
  const auto step = [&](const boost::regex &expect, const boost::regex &failure) {
    return await_line(socket, io_service, deadline, buffer, expect, failure);
  };
  // A read that produced no answer, for the binary protocols that do their own
  // framing instead of going through await_line().
  const auto stalled = [&failed](const read_outcome outcome) { return failed(step_from_read(outcome)); };

  switch (preset.kind) {
    case starttls::negotiation::line: {
      if (!patterns.greeting_expect.empty()) {
        const step_result greeted = step(patterns.greeting_expect, patterns.failure);
        if (greeted != step_result::matched) return failed(greeted);
      }
      if (preset.preamble[0] != '\0') {
        if (!write_all(socket, io_service, deadline, preset.preamble)) return refuse("starttls_write_failed");
        const step_result answered = step(patterns.preamble_expect, patterns.failure);
        if (answered != step_result::matched) return failed(answered);
      }
      if (!write_all(socket, io_service, deadline, preset.command)) return refuse("starttls_write_failed");
      const step_result upgraded = step(patterns.command_expect, patterns.failure);
      if (upgraded != step_result::matched) return failed(upgraded);
      return true;
    }
    case starttls::negotiation::postgres: {
      if (!write_all(socket, io_service, deadline, starttls::postgres_ssl_request())) return refuse("starttls_write_failed");
      while (buffer.empty()) {
        const read_outcome outcome = read_some_until(socket, io_service, deadline, buffer);
        if (outcome != read_outcome::data) return stalled(outcome);
      }
      // 'N' is a server built without TLS, or one with it turned off.
      return starttls::postgres_accepts(buffer[0]) ? true : refuse("starttls_refused");
    }
    case starttls::negotiation::mysql: {
      // The server speaks first, and the SSLRequest continues that packet's
      // sequence number, so the handshake packet has to be off the wire before
      // we answer.
      while (!starttls::mysql_handshake_complete(buffer)) {
        const read_outcome outcome = read_some_until(socket, io_service, deadline, buffer);
        if (outcome != read_outcome::data) return stalled(outcome);
      }
      // The server publishes CLIENT_SSL in that packet. Asking a server that
      // did not advertise it just gets the connection dropped, which would
      // read as a handshake failure rather than the plain "no TLS here" it is.
      if (!starttls::mysql_server_supports_ssl(buffer)) return refuse("starttls_refused");
      if (!write_all(socket, io_service, deadline, starttls::mysql_ssl_request())) return refuse("starttls_write_failed");
      // A MySQL server acknowledges an SSLRequest by starting the handshake,
      // so there is nothing to read here - the next bytes are already TLS. One
      // with TLS disabled closes instead, which the handshake below reports.
      return true;
    }
    case starttls::negotiation::ldap: {
      if (!write_all(socket, io_service, deadline, starttls::ldap_starttls_request())) return refuse("starttls_write_failed");
      for (;;) {
        const starttls::verdict answered = starttls::ldap_reply_verdict(buffer);
        if (answered == starttls::verdict::matched) return true;
        if (answered == starttls::verdict::failed) return refuse("starttls_refused");
        const read_outcome outcome = read_some_until(socket, io_service, deadline, buffer);
        if (outcome != read_outcome::data) return stalled(outcome);
      }
    }
  }
  return refuse("starttls_unsupported");
}

// A required name that nothing covered is the answer the check was asked for,
// so it outranks how the conversation went - but not a transport failure,
// which says the check never got far enough to judge anything. Mirrors the
// precedence check_http applies to the same option.
void apply_san_requirement(const bool sans_held, check_tcp_filter::filter_obj &out) {
  if (sans_held) return;
  if (out.result == "ok" || out.result == "no_match") out.result = "san_missing";
}

// Synchronous TCP connect with millisecond timeout. When use_tls is set a TLS
// handshake is performed after the TCP connect. Optionally writes "send_data"
// and reads the response; the raw response is stored in out.response (trimmed).
// If "expect" (substring) and/or "expect_regex" are given the result is set to
// "no_match" when the response fails either. "result" gets a short status word
// (ok/timeout/refused/no_match/tls_handshake_failed/error).
void run_tcp_check(const std::string &host, unsigned short port, const tcp_check_options &opt, check_tcp_filter::filter_obj &out) {
  out.host = host;
  out.port = port;
  out.connected = false;
  out.result = "error";
  out.time = 0;

  boost::asio::io_context io_service;
  tcp::resolver resolver(io_service);
  tcp::socket socket(io_service);

  const auto start = boost::chrono::steady_clock::now();
  boost::system::error_code connect_ec = boost::asio::error::would_block;

  try {
    boost::system::error_code resolve_ec;
    auto endpoints = net::resolve_for_family(resolver, opt.af, host, std::to_string(port), resolve_ec);
    if (resolve_ec || endpoints.empty()) {
      // With address-family pinned this also covers "the name exists but has no
      // address in the requested family", which is the answer the check is
      // being asked for rather than an internal error.
      out.result = "resolve_failed";
      return;
    }

    const bool connect_timed_out =
        run_with_deadline(socket, io_service, steady_clock::now() + std::chrono::milliseconds(opt.timeout_ms), [&](const auto &finished) {
          boost::asio::async_connect(socket, endpoints, [&](const boost::system::error_code &ec, const tcp::endpoint &) {
            connect_ec = ec;
            finished();
          });
        });

    const auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::steady_clock::now() - start).count();
    out.time = static_cast<long long>(elapsed);

    if (connect_timed_out) {
      out.result = "timeout";
      return;
    }
    if (connect_ec) {
      out.result = connect_ec == boost::asio::error::connection_refused ? "refused" : "error";
      return;
    }

    out.connected = true;
    out.result = "ok";

    // Opportunistic TLS: the upgrade is negotiated in the clear, so it happens
    // between the connect and the handshake. A refusal here is the answer the
    // check was asked for, and run_tcp_check's caller reads it off `result`.
    if (opt.starttls_preset != nullptr && !negotiate_starttls(socket, io_service, opt.timeout_ms, *opt.starttls_preset, out)) return;

    if (!opt.use_tls) {
      // A plain connection served no certificate, so a name required through
      // `sans=` is not covered by one - the same answer check_http gives a
      // redirect that lands on plain http. Reporting ok here would pass the
      // very check the option exists to fail.
      const bool sans_held = cert::require_without_certificate(out.cert, opt.required_sans);
      tcp_converse(socket, socket, io_service, opt.timeout_ms, opt.send_data, opt.expect, opt.expect_regex, out);
      apply_san_requirement(sans_held, out);
    } else {
#ifdef USE_SSL
      boost::asio::ssl::context ctx(socket_helpers::tls_method_parser(opt.tls_version));
      // A "1.2+" tls version resolves to the generic method plus a floor that
      // has to be applied separately, or the '+' silently means "any".
      socket_helpers::apply_tls_min_version(ctx, opt.tls_version);
      try {
        if (opt.ca_file.empty() || opt.ca_file == "none") {
          // No bundle configured: fall back to OpenSSL's own trust store so a
          // check against a publicly issued certificate verifies out of the
          // box. Without this the chain has nothing to anchor to and
          // cert_verify reads "unable to get local issuer certificate" for
          // every correctly configured server on the internet, which makes
          // `crit=cert_verify != 'ok'` fire on exactly the certificates it was
          // written to bless. The configured bundle is still preferred: on
          // Windows ${ca-path} is the exported ROOT store, and OpenSSL's
          // default paths do not include the Windows certificate store at all.
          ctx.set_default_verify_paths();
        } else {
          // Accepts a PEM bundle file or a hashed CA directory: /etc/ssl/certs
          // is the latter on every distribution, and is what an operator
          // reaches for.
          socket_helpers::load_verify_location(ctx, opt.ca_file);
        }
      } catch (const socket_helpers::socket_exception &e) {
        // `ca=` is a check argument, so the OpenSSL reason ("No such file or
        // directory", "Permission denied", "no start line") must not travel
        // back in the result: it answers "does this path exist and can the
        // service read it?" for any file the agent can reach. The exception
        // already splits the two - what() is the caller-safe half.
        if (e.has_detail()) NSC_LOG_ERROR_STD(e.detail());
        out.result = std::string("error: ") + e.what();
        return;
      } catch (const std::exception &e) {
        NSC_LOG_ERROR_STD(std::string("Failed to load CA ") + opt.ca_file + ": " + e.what());
        out.result = "error: failed to load the CA bundle (see the agent log for the reason)";
        return;
      }
      // The certificate as OpenSSL handed it to verification, kept because
      // SSL_get_peer_certificate() will not produce it afterwards - see where
      // the verify callback below fills it in. Declared ahead of ssl_stream so
      // it outlives the callback that stream owns and that refers to it.
      boost::optional<socket_helpers::peer_certificate> verified_leaf;

      // Wrap the already-connected socket by reference so we keep the timed
      // connect above and only layer TLS on top.
      boost::asio::ssl::stream<tcp::socket &> ssl_stream(socket, ctx);
      const boost::asio::ssl::verify_mode vmode = socket_helpers::verify_mode_parser(opt.verify_mode);
      ssl_stream.set_verify_mode(vmode);
      // sni= overrides both the name offered to a multi-certificate server and
      // the name the certificate is verified against, which is what lets a
      // check reach a virtual host by IP and still assert the right identity.
      const std::string tls_name = opt.sni.empty() ? host : opt.sni;
      if (!tls_name.empty()) SSL_set_tlsext_host_name(ssl_stream.native_handle(), tls_name.c_str());

      // SSL_get_peer_certificate() only returns a certificate once the chain
      // VERIFIED: under a verifying mode, one that is expired, self-signed or
      // issued by an unknown CA aborts the handshake before OpenSSL stores it.
      // Reading it after the failure therefore reports nothing about the very
      // certificate that was rejected - so `crit=ssl_expiry_days < 0` would go
      // quiet on an expired certificate, which is the case it exists for. The
      // verify callback runs before that decision, so the details are taken
      // there.
      //
      // Only installed when verification is on: at verify=none the handshake
      // completes and OpenSSL stores the certificate the ordinary way.
      if (vmode != boost::asio::ssl::verify_none) {
        const boost::asio::ssl::host_name_verification host_check(tls_name);
        ssl_stream.set_verify_callback([&verified_leaf, host_check](const bool preverified, boost::asio::ssl::verify_context &ctx) {
          // get0_cert is the certificate verification was ASKED about - the
          // leaf - whatever depth the callback is currently reporting on. A
          // chain that fails at depth 1 never calls back at depth 0, so
          // reading the current certificate instead would capture an
          // intermediate, or nothing at all.
          if (!verified_leaf) verified_leaf = socket_helpers::certificate_details(X509_STORE_CTX_get0_cert(ctx.native_handle()));
          return host_check(preverified, ctx);
        });
      }

      // Handshake with the same millisecond deadline as the connect.
      boost::system::error_code hs_ec = boost::asio::error::would_block;
      const bool hs_timed_out =
          run_with_deadline(socket, io_service, steady_clock::now() + std::chrono::milliseconds(opt.timeout_ms), [&](const auto &finished) {
            ssl_stream.async_handshake(boost::asio::ssl::stream_base::client, [&](const boost::system::error_code &ec) {
              hs_ec = ec;
              finished();
            });
          });

      // The peer certificate is readable whether or not the handshake completed,
      // and regardless of `verify` - so an expiry check needs no trust decision,
      // and a handshake that failed *because* of the chain can still say so.
      // The fallback is the rejected certificate captured during verification,
      // which is the only copy left once a verifying handshake refused it.
      auto info = socket_helpers::peer_certificate_details(ssl_stream.native_handle());
      if (!info) info = verified_leaf;

      // OpenSSL's verdict on the chain, which it records whether or not `verify`
      // asked it to enforce one - that is what lets an untrusted chain be
      // reported without refusing to connect.
      //
      // After a handshake that FAILED, X509_V_OK is ambiguous: OpenSSL reports
      // it both when the chain was fine and something else broke, and when
      // verification never ran at all (a reset, a timeout, a rejected TLS
      // version). Reporting "ok" there would stand a clean chain next to a
      // connection that never checked one, and `crit=cert_verify != 'ok'`
      // would stay quiet on every one of them. A verdict that is NOT ok is the
      // reason the handshake failed, and is exactly what the keyword is for -
      // so it is kept, and only the ambiguous "ok" is dropped.
      if ((!hs_ec && !hs_timed_out) || !socket_helpers::peer_verify_ok(ssl_stream.native_handle()))
        out.cert.verify_result = socket_helpers::peer_verify_result(ssl_stream.native_handle());

      // Populated BEFORE the handshake verdict is acted on, because a handshake
      // that failed over the certificate is exactly when its details are worth
      // reporting: an expired certificate under verify=peer fails the
      // handshake, and returning here with cert_* empty would leave
      // `crit=ssl_expiry_days < 0` silent on the one case it was written for.
      //
      // No certificate at all fails a `sans=` requirement the same way: it is
      // the case the option exists to catch, and staying silent would report ok.
      const bool sans_held = info ? cert::populate(out.cert, info.value(), opt.required_sans)
                                  : cert::require_without_certificate(out.cert, opt.required_sans);

      if (hs_timed_out) {
        out.result = "tls_handshake_timeout";
        return;
      }
      if (hs_ec) {
        // The more specific answer, and it keeps its word: a name the
        // certificate does not carry is already visible in missing_sans, and
        // the handshake is why the check got no further.
        out.result = "tls_handshake_failed";
        return;
      }

      tcp_converse(ssl_stream, socket, io_service, opt.timeout_ms, opt.send_data, opt.expect, opt.expect_regex, out);
      apply_san_requirement(sans_held, out);
#else
      out.result = "error: TLS requested but this build has no TLS support";
      return;
#endif
    }
  } catch (const std::exception &e) {
    out.result = std::string("error: ") + check_net::format_exception_message(e);
  }

  boost::system::error_code ignore;
  socket.close(ignore);
}

}  // namespace

namespace {
// Shared core for check_tcp and check_ssh. When `forced` is non-null (check_ssh)
// its preset is always applied; otherwise the preset is chosen from a `service`
// argument. FilterT/ObjT let check_ssh plug in its own object, which adds the
// parsed identification string on top of the TCP fields.
template <typename FilterT, typename ObjT>
void check_tcp_impl(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                    PB::Commands::QueryResponseMessage::Response *response, const service_preset *forced) {
  typedef FilterT filter;
  typedef ObjT filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  std::vector<std::string> hosts;
  std::string hosts_string;
  unsigned short port = 0;
  std::string service;
  std::string starttls_protocol;
  std::string required_sans;
  std::string address_family_arg;
  tcp_check_options opt;

  // The supported list comes from the preset table, so the help can never
  // drift from what find_preset() accepts. program_options copies the text,
  // but it takes a const char*, hence the named local.
  const std::string starttls_help =
      "Upgrade the plaintext connection to TLS with the protocol's own STARTTLS negotiation, then check the certificate: " +
      starttls::supported_protocols() +
      ". Implies ssl=true and sets the protocol's default plaintext port. Use this for the services that have no implicit-TLS port "
      "(submission/587, LDAP, PostgreSQL, MySQL).";

  filter f;
  filter_helper.add_options("time > 1000", "time > 5000 or result != 'ok'", "", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${host}:${port} ${result} in ${time}ms", "${host}_${port}", "No hosts checked", "%(status): %(list)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::vector<std::string> >(&hosts), "Host(s) to connect to (may be given multiple times).")
    ("hosts", po::value<std::string>(&hosts_string), "Comma separated list of hosts to connect to.")
    ("port", po::value<unsigned short>(&port), "TCP port to connect to.")
    ("timeout", po::value<int>(&opt.timeout_ms)->default_value(5000), "Connection / read timeout in milliseconds.")
    ("send", po::value<std::string>(&opt.send_data), "Optional payload to send after the connection is established.")
    ("expect", po::value<std::string>(&opt.expect), "Optional substring expected in the response.")
    ("ssl", po::value<bool>(&opt.use_tls)->implicit_value(true)->default_value(false), "Wrap the connection in TLS/SSL after connecting (ssl=true).")
    ("starttls", po::value<std::string>(&starttls_protocol), starttls_help.c_str())
    ("tls-version", po::value<std::string>(&opt.tls_version)->default_value("tlsv1.2+"),
        "TLS version when --ssl is used (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).")
    ("verify", po::value<std::string>(&opt.verify_mode)->default_value("peer"),
        "Certificate verify mode when the connection is TLS: peer (default; the chain must be trusted and the name must match), none, "
        "peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate. Set verify=none to reach a server whose certificate does not "
        "validate - cert_verify still reports why it did not.")
    ("ca", po::value<std::string>(&opt.ca_file)->default_value(default_ca_file),
        "Trust anchor used to verify the server certificate: either a PEM bundle file or a hashed CA directory such as /etc/ssl/certs. "
        "Defaults to the agent's configured bundle (${ca-path}); when that is empty or 'none' the system trust store OpenSSL was built "
        "with is used instead.")
    ("sni", po::value<std::string>(&opt.sni),
        "TLS Server Name Indication: the name offered to a server hosting several certificates, and the name the certificate is verified "
        "against. Defaults to the host connected to; set it to check a virtual host reached by IP.")
    ("sans", po::value<std::string>(&required_sans),
        "Comma separated names the certificate must cover through subjectAltName, e.g. www.example.com,example.com. Wildcard entries match "
        "one label (*.example.com covers www.example.com). A missing name sets result=san_missing and lists it in the missing_sans keyword.")
    ("address-family", po::value<std::string>(&address_family_arg), net::address_family_option_help())
    ;
  if (forced == nullptr) {
    filter_helper.get_desc().add_options()
      ("service", po::value<std::string>(&service),
          "Service preset (ftp, pop, imap, smtp, ssh, spop, simap, ssmtp): sets a default port, greeting and expected-response regex. "
          "The s-prefixed variants use implicit TLS.")
      ;
  }
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!net::parse_address_family(address_family_arg, opt.af))
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid address-family: " + address_family_arg + " (expected any, ipv4 or ipv6)");

  if (!starttls_protocol.empty()) {
    opt.starttls_preset = starttls::find_preset(starttls_protocol);
    if (opt.starttls_preset == nullptr)
      return nscapi::protobuf::functions::set_response_bad(
          *response, "Unknown starttls protocol: " + starttls_protocol + " (supported: " + starttls::supported_protocols() + ")");
    // The point of STARTTLS is to end up in TLS, so it implies it rather than
    // making the caller say both.
    opt.use_tls = true;
    if (port == 0) port = opt.starttls_preset->port;
  }
  opt.required_sans = cert::parse_required_sans(required_sans);

  // Resolve the preset: forced (check_ssh) or from the `service` argument.
  const service_preset *preset = forced;
  if (preset == nullptr && !service.empty()) {
    preset = find_service_preset(service);
    if (preset == nullptr) return nscapi::protobuf::functions::set_response_bad(*response, "Unknown service preset: " + service);
  }

  // A service preset describes a conversation that starts with the server's
  // greeting; after a STARTTLS upgrade that greeting has already been consumed
  // in the clear and is never sent again. Applying the preset's expect on top
  // would wait out the whole timeout and report read_failed against a server
  // that is working perfectly, so refuse the combination rather than fail
  // mysteriously. (`forced` is check_ssh, which is never a STARTTLS service.)
  if (opt.starttls_preset != nullptr && preset != nullptr)
    return nscapi::protobuf::functions::set_response_bad(
        *response, "starttls= cannot be combined with the '" + std::string(preset->name) +
                       "' service preset: the preset waits for a greeting that is not sent again after the upgrade. Use one or the other.");

  if (preset != nullptr) {
    if (port == 0) port = preset->port;
    if (opt.send_data.empty()) opt.send_data = preset->send;
    opt.expect_regex = preset->expect_regex;
    if (preset->tls) opt.use_tls = true;
  }

  // sni= names the certificate to ask for and to verify against, so outside a
  // TLS session there is nothing for it to do. Silently ignoring it is the
  // failure mode worth avoiding: the check would come back OK having asserted
  // nothing, which reads exactly like a passing identity check. Refuse it
  // instead and say which option turns TLS on. (`sans=` needs no such guard -
  // a connection that served no certificate does not cover a required name,
  // and reports san_missing.)
  if (!opt.sni.empty() && !opt.use_tls)
    return nscapi::protobuf::functions::set_response_bad(
        *response, "sni= only applies to a TLS connection: add ssl=true, a starttls= protocol, or one of the implicit-TLS service presets.");

  if (!hosts_string.empty()) {
    std::vector<std::string> tmp;
    boost::split(tmp, hosts_string, boost::is_any_of(","));
    for (auto &h : tmp) {
      boost::trim(h);
      if (!h.empty()) hosts.push_back(h);
    }
  }

  if (hosts.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No host specified");
  if (port == 0) return nscapi::protobuf::functions::set_response_bad(*response, "No port specified");

  if (!filter_helper.build_filter(f)) return;

  for (const auto &host : hosts) {
    auto obj = std::make_shared<filter_obj>();
    run_tcp_check(host, port, opt, *obj);
    obj->post_read();
    f.match(obj);
  }

  filter_helper.post_process(f);
}
}  // namespace

void check_tcp(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
               PB::Commands::QueryResponseMessage::Response *response) {
  check_tcp_impl<check_tcp_filter::filter, check_tcp_filter::filter_obj>(default_ca_file, request, response, nullptr);
}

void check_ssh(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
               PB::Commands::QueryResponseMessage::Response *response) {
  check_tcp_impl<check_ssh_filter::filter, check_ssh_filter::filter_obj>(default_ca_file, request, response, find_service_preset("SSH"));
}

}  // namespace check_net
