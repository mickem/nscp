// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/regex.hpp>
#include <string>
#include <vector>

namespace check_net {
// Opportunistic TLS: the protocol-specific conversation that upgrades an
// already-connected plaintext socket to TLS, so a certificate check can reach
// the services that have no implicit-TLS port (submission/587, LDAP/389,
// PostgreSQL, MySQL, ...).
//
// Everything here is pure: the presets and the parsing/rendering of each step.
// The socket driving lives in check_tcp.cpp, which keeps this unit-testable
// without a server to talk to.
namespace starttls {

// How a protocol negotiates the upgrade.
enum class negotiation {
  // A line-oriented text protocol: read the greeting, optionally send a
  // preamble (SMTP's EHLO) and wait for its reply, send the upgrade command
  // and wait for the go-ahead. Every step is "read lines until one matches a
  // regex", which is what makes multi-line replies (250-/250 ) and chatty
  // servers (IRC numerics) fall out for free.
  line,
  // A single fixed request, then one byte: 'S' to proceed, 'N' to refuse.
  postgres,
  // Read the server's handshake packet, then send an SSLRequest packet. The
  // server sends nothing back - the next byte on the wire is already TLS.
  mysql,
  // A BER-encoded LDAP extended request; the reply carries a resultCode.
  ldap,
};

// One protocol's conversation. The *_expect fields are regular expressions
// matched against each received line, not prefixes: it is what lets `irc`
// wait for a numeric in the middle of a line while `smtp` anchors at the
// start, with one engine.
struct preset {
  const char *name;
  // Default port when the caller gave none. Matches the plaintext port the
  // protocol upgrades from, never the implicit-TLS one.
  unsigned short port;
  negotiation kind;
  // Line the server is expected to greet with. Empty means "send without
  // waiting" (IRC has no greeting before registration).
  const char *greeting_expect;
  // Optional line sent, and awaited, before the upgrade command.
  const char *preamble;
  const char *preamble_expect;
  const char *command;
  const char *command_expect;
  // A received line matching this ends the negotiation as refused. Checked
  // before the expect, so a protocol whose error replies share a shape with
  // its success replies still fails fast instead of waiting out the timeout.
  const char *failure_regex;
};

// Look up a protocol by name, case-insensitively. nullptr when unknown.
const preset *find_preset(const std::string &name);

// The supported protocol names, space separated, for an error message.
std::string supported_protocols();

// A preset's patterns, compiled. Built once per check and handed to
// classify_line for every line: compiling inside the classifier turned each
// received line into two regex constructions, and a peer is free to send lines
// until the negotiation budget runs out.
//
// An invalid pattern (only reachable from a malformed preset) compiles to an
// empty regex, which classify_line treats as "no match" - a typo in the table
// degrades to a timeout rather than throwing mid-check.
struct compiled_preset {
  boost::regex greeting_expect;
  boost::regex preamble_expect;
  boost::regex command_expect;
  boost::regex failure;
};
compiled_preset compile(const preset &p);

// What one received line means for the step being awaited.
enum class verdict {
  pending,  // neither an answer nor an error: keep reading
  matched,  // the step succeeded
  failed,   // the server refused
};

// Classify a single received line against an already-compiled pair of
// patterns. An empty regex never matches.
verdict classify_line(const std::string &line, const boost::regex &expect, const boost::regex &failure);

// Pull the complete lines out of a receive buffer, leaving any trailing
// partial line behind for the next read. Handles CRLF and bare LF; the
// returned lines carry neither.
std::vector<std::string> take_complete_lines(std::string &buffer);

// PostgreSQL SSLRequest: an 8-byte startup packet whose "version" is the magic
// 80877103. The reply is one byte.
std::string postgres_ssl_request();
bool postgres_accepts(char reply);

// MySQL SSLRequest: the first half of a HandshakeResponse41 - capability
// flags with CLIENT_SSL set, and nothing after the reserved block, which is
// what tells the server the rest of the handshake arrives over TLS. Sent as
// sequence 1, immediately after the server's own handshake packet (sequence
// 0).
std::string mysql_ssl_request();
// Whether a MySQL server handshake packet is complete: a 4-byte header whose
// first three bytes are the little-endian payload length.
bool mysql_handshake_complete(const std::string &buffer);

// Whether a complete server handshake packet advertises CLIENT_SSL. A MySQL
// server built or configured without TLS answers an SSLRequest by dropping the
// connection, which would surface as a bare handshake failure; reading the
// capability the server itself published turns that into a plain "refused",
// and is what `openssl s_client -starttls mysql` does too. False for a packet
// too short or too malformed to carry the flags - a server that did not say it
// supports TLS is not asked for it.
bool mysql_server_supports_ssl(const std::string &handshake_packet);

// LDAP StartTLS: the extended request carrying OID 1.3.6.1.4.1.1466.20037.
std::string ldap_starttls_request();
// What an LDAP reply buffer says so far. Pending until the *whole* LDAPMessage
// has arrived - judging on the resultCode alone would leave the rest of the
// PDU in the kernel buffer, where the TLS handshake then reads it as a record
// and fails against a healthy server.
verdict ldap_reply_verdict(const std::string &buffer);

// Total size of the BER element at the start of `buffer`, header included.
// False when the length header has not fully arrived, or carries an encoding
// LDAP never uses (indefinite length, or one too large to represent).
bool ber_element_length(const std::string &buffer, std::size_t &total);

}  // namespace starttls
}  // namespace check_net
