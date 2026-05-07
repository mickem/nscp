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

#pragma once

#include <stdexcept>
#include <string>

namespace smtp {

// Synchronous SMTP submission client. Used by the SMTPClient module to
// deliver one notification per call. Designed to interoperate with Gmail,
// Microsoft 365 and any RFC 5321-compliant submission service.
//
// The implementation uses Boost.Asio synchronously - the previous async
// queue model never actually worked end-to-end and was substantially
// harder to reason about than a per-submission "connect, send, quit" flow.
// One submission per call also matches how SMTPClient::submit() is invoked
// from the rest of the agent.
//
// Security defaults that matter:
//   * security="starttls" by default (port 587 submission), upgrade to TLS
//     before auth.
//   * security="tls" forces implicit TLS from connect (port 465).
//   * security="none" is permitted for self-hosted internal relays only.
//   * AUTH PLAIN / AUTH LOGIN are supported and always wrapped in TLS
//     (we refuse to send credentials in clear).
//   * Envelope addresses and header values are validated against CRLF
//     injection.
//   * DATA payload is dot-stuffed per RFC 5321 5.2.7.
class smtp_exception : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct connection_config {
  std::string server;            // hostname/IP of the SMTP server
  std::string port = "587";      // 25, 465, 587, ...
  std::string security = "starttls";  // "none" | "starttls" | "tls"
  std::string username;          // AUTH username (empty = no AUTH)
  std::string password;          // AUTH password
  std::string canonical_name;    // EHLO hostname; defaults to "localhost"
  bool insecure_skip_verify = false;  // for self-signed test servers
  int timeout_seconds = 30;
};

struct message {
  std::string from;     // envelope sender, also used as From: header
  std::string to;       // single recipient (one message per submission)
  std::string subject;  // header
  std::string body;     // arbitrary text; CRLF normalisation done internally
};

// Send one message synchronously. Throws smtp_exception on any failure
// (DNS, connect, TLS, AUTH, server rejection, protocol error). The error
// message is suitable for surfacing via NSC_LOG_ERROR.
void send(const connection_config& cfg, const message& msg);

// Internals exposed for unit testing. These are the security-critical
// transformations that protect against CRLF injection in addresses and
// headers, and that produce a wire-correct DATA payload (dot-stuffing
// per RFC 5321 5.2.7, CRLF normalisation). They are pure functions; tests
// can exercise them without standing up a real SMTP server.
namespace detail {
// Throws smtp_exception if `addr` is empty, contains a control character,
// or contains an angle bracket. Used for envelope addresses.
void validate_address(const std::string& addr, const char* what);

// Strips CR / LF / NUL from a header value so it cannot inject extra
// header lines. Returns the cleaned string.
std::string sanitise_header(const std::string& v);

// Applies RFC 5321 transparency (lines starting with "." are doubled) and
// normalises lone CR / LF to CRLF. Used for the DATA payload.
std::string dot_stuff_and_crlf(const std::string& body);
}  // namespace detail

}  // namespace smtp
