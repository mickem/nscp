// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <string>

namespace collectd {
namespace crypto {

// The security levels of the collectd network protocol (the network plugin's
// SecurityLevel option): plaintext, an HMAC-SHA-256 signature part (0x0200)
// prepended to the packet, or the whole packet wrapped in an AES-256/OFB
// encryption part (0x0210). Sign and Encrypt authenticate the sender against
// a username/password entry in the server's AuthFile.
enum class security_level { none, sign, encrypt };

// Parse a configured security level ("none", "sign" or "encrypt",
// case-insensitive; empty means none). Returns false on any other value.
bool parse_security_level(const std::string &value, security_level &level);

// Human-readable name for logs.
std::string to_string(security_level level);

// True when built with OpenSSL (USE_SSL); without it sign/encrypt MUST fail
// closed rather than silently falling back to plaintext.
bool available();

// The per-datagram bytes a level's wrapping adds in front of the payload:
// the packet budget must shrink by this much so wrapped datagrams still fit
// the receiver's read size (the network plugin's MaxPacketSize).
std::size_t overhead(security_level level, const std::string &username);

// Wrap a fully rendered plaintext packet in a signature part:
//   type(2)=0x0200, length(2)=36+len(user), HMAC-SHA-256(32), username
// followed by the untouched packet. The HMAC key is the password and the
// authenticated data is username || packet, matching collectd's network.c.
// On failure returns false and sets error; out is only valid on success.
bool sign_packet(const std::string &packet, const std::string &username, const std::string &password, std::string &out, std::string &error);

// Wrap a fully rendered plaintext packet in an encryption part:
//   type(2)=0x0210, length(2)=42+len(user)+len(packet), len(user)(2),
//   username, IV(16), then AES-256/OFB( SHA-1(packet) || packet )
// with the key SHA-256(password), matching collectd's network.c.
// On failure returns false and sets error; out is only valid on success.
bool encrypt_packet(const std::string &packet, const std::string &username, const std::string &password, std::string &out, std::string &error);

}  // namespace crypto
}  // namespace collectd
