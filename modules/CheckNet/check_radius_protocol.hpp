// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef USE_SSL
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif

namespace check_net {
namespace radius {
using packet = std::vector<unsigned char>;
using digest = std::array<unsigned char, 16>;

#ifdef USE_SSL
inline digest md5(const packet &data) {
  digest out{};
  unsigned int size = 0;
  if (EVP_Digest(data.data(), data.size(), out.data(), &size, EVP_md5(), nullptr) != 1 || size != out.size())
    throw std::runtime_error("RADIUS MD5 unavailable in this OpenSSL configuration");
  return out;
}

inline digest hmac(const packet &data, const std::string &secret) {
  digest out{};
  unsigned int size = 0;
  if (!HMAC(EVP_md5(), secret.data(), static_cast<int>(secret.size()), data.data(), data.size(), out.data(), &size) || size != out.size())
    throw std::runtime_error("RADIUS HMAC-MD5 unavailable in this OpenSSL configuration");
  return out;
}

inline void attribute(packet &p, unsigned char type, const packet &value) {
  if (value.size() > 253) throw std::runtime_error("RADIUS attribute exceeds 253 bytes");
  p.push_back(type);
  p.push_back(static_cast<unsigned char>(value.size() + 2));
  p.insert(p.end(), value.begin(), value.end());
}

// The caller supplies randomness so RFC vectors can exercise the real encoder.
inline packet request(unsigned char id, const digest &authenticator, const std::string &secret, const std::string &user, const std::string &password,
                      bool status, const std::string &nas) {
  if (secret.empty() || secret.size() > 4096) throw std::runtime_error("RADIUS secret must contain 1..4096 bytes");
  if (password.size() > 128) throw std::runtime_error("RADIUS PAP password exceeds 128 bytes");
  packet p(20, 0);
  p[0] = status ? 12 : 1;
  p[1] = id;
  std::copy(authenticator.begin(), authenticator.end(), p.begin() + 4);
  if (!status) {
    if (user.empty()) throw std::runtime_error("RADIUS username is required");
    attribute(p, 1, packet(user.begin(), user.end()));
    packet encrypted(std::max<std::size_t>(16, ((password.size() + 15) / 16) * 16), 0);
    std::copy(password.begin(), password.end(), encrypted.begin());
    digest previous = authenticator;
    for (std::size_t offset = 0; offset < encrypted.size(); offset += 16) {
      packet input(secret.begin(), secret.end());
      input.insert(input.end(), previous.begin(), previous.end());
      const digest hash = md5(input);
      for (std::size_t i = 0; i < 16; ++i) encrypted[offset + i] ^= hash[i];
      std::copy_n(encrypted.begin() + offset, 16, previous.begin());
    }
    attribute(p, 2, encrypted);
  }
  attribute(p, 32, packet(nas.begin(), nas.end()));
  attribute(p, 80, packet(16, 0));
  p[2] = static_cast<unsigned char>(p.size() >> 8);
  p[3] = static_cast<unsigned char>(p.size());
  const digest message_authenticator = hmac(p, secret);
  std::copy(message_authenticator.begin(), message_authenticator.end(), p.end() - 16);
  return p;
}

inline packet random_request(const std::string &secret, const std::string &user, const std::string &password, bool status, const std::string &nas) {
  digest authenticator{};
  unsigned char id = 0;
  if (RAND_bytes(authenticator.data(), static_cast<int>(authenticator.size())) != 1 || RAND_bytes(&id, 1) != 1)
    throw std::runtime_error("RADIUS random number generation failed");
  return request(id, authenticator, secret, user, password, status, nas);
}

// Authenticate both the response and its Message-Authenticator before exposing
// a reply code. Trailing UDP padding is ignored as required by RFC 2865.
inline std::string validate(packet p, const packet &req, const std::string &secret) {
  if (req.size() < 20 || p.size() < 20) return "short_response";
  const std::size_t length = (static_cast<std::size_t>(p[2]) << 8) | p[3];
  if (length < 20 || length > 4096 || length > p.size()) return "invalid_length";
  p.resize(length);
  if (p[1] != req[1]) return "wrong_identifier";
  if (p[0] != 2 && p[0] != 3 && p[0] != 11 && !(req[0] == 12 && p[0] == 5)) return "invalid_code";
  std::size_t ma = 0;
  for (std::size_t offset = 20; offset < p.size();) {
    if (offset + 2 > p.size() || p[offset + 1] < 2 || offset + p[offset + 1] > p.size()) return "invalid_attribute";
    if (p[offset] == 80) {
      if (p[offset + 1] != 18 || ma != 0) return "invalid_message_authenticator";
      ma = offset + 2;
    }
    offset += p[offset + 1];
  }
  if (!ma) return "missing_message_authenticator";
  digest received{};
  std::copy_n(p.begin() + 4, 16, received.begin());
  std::copy_n(req.begin() + 4, 16, p.begin() + 4);
  packet signed_response = p;
  signed_response.insert(signed_response.end(), secret.begin(), secret.end());
  const digest response_auth = md5(signed_response);
  if (CRYPTO_memcmp(received.data(), response_auth.data(), 16) != 0) return "invalid_authenticator";
  std::copy_n(p.begin() + ma, 16, received.begin());
  std::fill_n(p.begin() + ma, 16, 0);
  const digest message_auth = hmac(p, secret);
  if (CRYPTO_memcmp(received.data(), message_auth.data(), 16) != 0) return "invalid_message_authenticator";
  return "";
}
#endif

inline std::string reply_name(unsigned char code) {
  switch (code) {
    case 2:
      return "access_accept";
    case 3:
      return "access_reject";
    case 5:
      return "accounting_response";
    case 11:
      return "access_challenge";
    default:
      return "none";
  }
}
inline bool expected(const std::string &mode, unsigned char code) {
  if (mode == "auth") return code == 2;
  if (mode == "reject") return code == 3;
  return code == 2 || code == 3 || code == 5;
}
}  // namespace radius
}  // namespace check_net
