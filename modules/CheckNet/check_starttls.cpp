// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_starttls.hpp"

#include <boost/regex.hpp>
#include <cstdint>
#include <string>

namespace check_net {
namespace starttls {

namespace {

// The conversations, one row per protocol.
//
// The EHLO/LHLO name is a literal rather than the local hostname: a STARTTLS
// probe is not delivering mail, and no server worth checking refuses the
// upgrade over the name in the greeting. Sending a resolved hostname would
// leak the monitoring host's name to every server checked.
const preset presets[] = {
    // SMTP's 250 reply is multi-line: the continuation lines are `250-`, the
    // last is `250 `. Anchoring the expect on the space is what consumes the
    // whole capability list before STARTTLS is sent.
    {"smtp", 25, negotiation::line, "^220", "EHLO nscp\r\n", "^250[ ]", "STARTTLS\r\n", "^220", "^[45][0-9][0-9]"},
    {"lmtp", 24, negotiation::line, "^220", "LHLO nscp\r\n", "^250[ ]", "STARTTLS\r\n", "^220", "^[45][0-9][0-9]"},
    {"pop3", 110, negotiation::line, "^\\+OK", "", "", "STLS\r\n", "^\\+OK", "^-ERR"},
    // The tag is ours, echoed back by the server on the reply to that command.
    {"imap", 143, negotiation::line, "^\\*\\s+OK", "", "", "a001 STARTTLS\r\n", "^a001\\s+OK", "^a001\\s+(NO|BAD)"},
    {"ftp", 21, negotiation::line, "^220", "", "", "AUTH TLS\r\n", "^234", "^[45][0-9][0-9]"},
    {"nntp", 119, negotiation::line, "^20[01]", "", "", "STARTTLS\r\n", "^382", "^[45][0-9][0-9]"},
    // ManageSieve greets with a capability list terminated by OK.
    {"sieve", 4190, negotiation::line, "^OK", "", "", "STARTTLS\r\n", "^OK", "^(NO|BYE)"},
    // IRC sends nothing until the client registers, so there is no greeting to
    // wait for; the 670 ("STARTTLS successful") numeric arrives mid-line after
    // the server prefix, which is why the expects are searches, not anchors.
    // 691 is "STARTTLS failed", 421 "unknown command" on a server without it.
    {"irc", 6667, negotiation::line, "", "", "", "STARTTLS\r\n", "\\s670\\s", "\\s(691|421)\\s"},
    {"postgres", 5432, negotiation::postgres, "", "", "", "", "", ""},
    {"mysql", 3306, negotiation::mysql, "", "", "", "", "", ""},
    {"ldap", 389, negotiation::ldap, "", "", "", "", "", ""},
};

char lower(const char c) {
  if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
  return c;
}

}  // namespace

const preset *find_preset(const std::string &name) {
  std::string needle = name;
  for (char &c : needle) c = lower(c);
  for (const preset &p : presets)
    if (needle == p.name) return &p;
  return nullptr;
}

std::string supported_protocols() {
  std::string result;
  for (const preset &p : presets) {
    if (!result.empty()) result += " ";
    result += p.name;
  }
  return result;
}

verdict classify_line(const std::string &line, const std::string &expect_regex, const std::string &failure_regex) {
  try {
    // Failure first: a protocol whose refusal shares a shape with its
    // go-ahead must not be read as success.
    if (!failure_regex.empty() && boost::regex_search(line, boost::regex(failure_regex, boost::regex::icase))) return verdict::failed;
    if (!expect_regex.empty() && boost::regex_search(line, boost::regex(expect_regex, boost::regex::icase))) return verdict::matched;
  } catch (const std::exception &) {
    // Only reachable from a malformed preset; degrade to "keep reading" so the
    // check times out with a protocol message rather than an exception.
    return verdict::pending;
  }
  return verdict::pending;
}

std::vector<std::string> take_complete_lines(std::string &buffer) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  for (;;) {
    const std::size_t nl = buffer.find('\n', start);
    if (nl == std::string::npos) break;
    std::size_t end = nl;
    if (end > start && buffer[end - 1] == '\r') end--;
    lines.emplace_back(buffer, start, end - start);
    start = nl + 1;
  }
  buffer.erase(0, start);
  return lines;
}

std::string postgres_ssl_request() {
  // int32 length (8, including itself) then int32 80877103 (0x04D2162F), both
  // big-endian: the "SSLRequest" pseudo startup packet.
  static const unsigned char packet[8] = {0x00, 0x00, 0x00, 0x08, 0x04, 0xD2, 0x16, 0x2F};
  return std::string(reinterpret_cast<const char *>(packet), sizeof(packet));
}

bool postgres_accepts(const char reply) { return reply == 'S'; }

std::string mysql_ssl_request() {
  // CLIENT_LONG_PASSWORD | CLIENT_PROTOCOL_41 | CLIENT_SSL |
  // CLIENT_SECURE_CONNECTION | CLIENT_PLUGIN_AUTH. CLIENT_PROTOCOL_41 is what
  // makes this the 32-byte form the server expects; CLIENT_SSL is the request
  // itself.
  const std::uint32_t capabilities = 0x00000001u | 0x00000200u | 0x00000800u | 0x00008000u | 0x00080000u;
  const std::uint32_t max_packet = 16777216u;

  std::string payload;
  payload.reserve(32);
  for (int i = 0; i < 4; i++) payload.push_back(static_cast<char>((capabilities >> (8 * i)) & 0xFF));
  for (int i = 0; i < 4; i++) payload.push_back(static_cast<char>((max_packet >> (8 * i)) & 0xFF));
  payload.push_back(static_cast<char>(45));  // utf8mb4_general_ci
  payload.append(23, '\0');                  // reserved, must be zero

  std::string packet;
  packet.reserve(4 + payload.size());
  const std::size_t length = payload.size();
  packet.push_back(static_cast<char>(length & 0xFF));
  packet.push_back(static_cast<char>((length >> 8) & 0xFF));
  packet.push_back(static_cast<char>((length >> 16) & 0xFF));
  // Sequence 1: the server's handshake was 0, and a MySQL server drops a
  // packet whose sequence does not continue the exchange.
  packet.push_back(static_cast<char>(1));
  packet += payload;
  return packet;
}

bool mysql_handshake_complete(const std::string &buffer) {
  if (buffer.size() < 4) return false;
  const std::size_t length = static_cast<unsigned char>(buffer[0]) | (static_cast<std::size_t>(static_cast<unsigned char>(buffer[1])) << 8) |
                             (static_cast<std::size_t>(static_cast<unsigned char>(buffer[2])) << 16);
  return buffer.size() >= 4 + length;
}

bool mysql_server_supports_ssl(const std::string &handshake_packet) {
  if (!mysql_handshake_complete(handshake_packet)) return false;
  // Protocol::HandshakeV10, inside the 4-byte packet header:
  //   1  protocol version (always 10)
  //   n  server version, NUL terminated
  //   4  connection id
  //   8  auth-plugin-data-part-1
  //   1  filler
  //   2  capability_flags_1  <- the low half, where CLIENT_SSL lives
  const std::size_t body = 4;
  if (handshake_packet.size() <= body) return false;
  if (static_cast<unsigned char>(handshake_packet[body]) != 0x0A) return false;
  const std::size_t nul = handshake_packet.find('\0', body + 1);
  if (nul == std::string::npos) return false;
  const std::size_t flags = nul + 1 + 4 + 8 + 1;
  if (flags + 1 >= handshake_packet.size()) return false;
  const unsigned int capabilities =
      static_cast<unsigned char>(handshake_packet[flags]) | (static_cast<unsigned int>(static_cast<unsigned char>(handshake_packet[flags + 1])) << 8);
  return (capabilities & 0x0800u) != 0;  // CLIENT_SSL
}

std::string ldap_starttls_request() {
  // LDAPMessage ::= SEQUENCE { messageID 1, extendedReq [APPLICATION 23] {
  //   requestName [0] "1.3.6.1.4.1.1466.20037" } }
  static const char oid[] = "1.3.6.1.4.1.1466.20037";
  const std::size_t oid_length = sizeof(oid) - 1;  // 22

  std::string request;
  request.push_back(static_cast<char>(0x30));                    // SEQUENCE
  request.push_back(static_cast<char>(3 + 2 + 2 + oid_length));  // messageID(3) + [APPLICATION 23] header(2) + [0] header(2) + OID
  request.push_back(static_cast<char>(0x02));                    // INTEGER
  request.push_back(static_cast<char>(0x01));
  request.push_back(static_cast<char>(0x01));  // messageID = 1
  request.push_back(static_cast<char>(0x77));  // [APPLICATION 23] ExtendedRequest
  request.push_back(static_cast<char>(2 + oid_length));
  request.push_back(static_cast<char>(0x80));  // [0] requestName
  request.push_back(static_cast<char>(oid_length));
  request.append(oid, oid_length);
  return request;
}

verdict ldap_reply_verdict(const std::string &buffer) {
  // The full reply is an LDAPMessage wrapping an ExtendedResponse
  // ([APPLICATION 24] = 0x78) whose first component is the resultCode, an
  // ENUMERATED (0x0A) of length 1. Rather than decoding BER properly we look
  // for that three-byte shape right after the response tag, which is the only
  // place it can occur in a well-formed StartTLS reply. Anything else keeps
  // the caller reading until the deadline.
  const std::size_t response = buffer.find(static_cast<char>(0x78));
  if (response == std::string::npos) return verdict::pending;
  for (std::size_t i = response + 1; i + 2 < buffer.size(); i++) {
    if (static_cast<unsigned char>(buffer[i]) != 0x0A) continue;
    if (static_cast<unsigned char>(buffer[i + 1]) != 0x01) continue;
    return static_cast<unsigned char>(buffer[i + 2]) == 0x00 ? verdict::matched : verdict::failed;
  }
  return verdict::pending;
}

}  // namespace starttls
}  // namespace check_net
