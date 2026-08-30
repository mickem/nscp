// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collectd_crypto.hpp"

#include "collectd_packet.hpp"

#ifdef USE_SSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif

#include <boost/algorithm/string/case_conv.hpp>

namespace {
// Part type codes (network.h: TYPE_SIGN_SHA256 / TYPE_ENCR_AES256).
const int16_t part_sign_sha256 = 0x0200;
const int16_t part_encr_aes256 = 0x0210;

// Fixed sizes of the two parts excluding the username (network.c:
// PART_SIGNATURE_SHA256_SIZE / PART_ENCRYPTION_AES256_SIZE).
const std::size_t sign_part_size = 36;   // head(4) + hmac(32)
const std::size_t encr_part_size = 42;   // head(4) + user_len(2) + iv(16) + sha1(20)

// collectd's own sender rejects usernames that would not leave room in its
// 256-byte signature buffer; mirror that cap so we never emit a part the
// reference implementation could not have produced.
const std::size_t max_username_length = 256 - sign_part_size;
}  // namespace

bool collectd::crypto::parse_security_level(const std::string &value, security_level &level) {
  const std::string v = boost::algorithm::to_lower_copy(value);
  if (v.empty() || v == "none") {
    level = security_level::none;
  } else if (v == "sign") {
    level = security_level::sign;
  } else if (v == "encrypt") {
    level = security_level::encrypt;
  } else {
    return false;
  }
  return true;
}

std::string collectd::crypto::to_string(security_level level) {
  switch (level) {
    case security_level::sign:
      return "sign";
    case security_level::encrypt:
      return "encrypt";
    default:
      return "none";
  }
}

std::size_t collectd::crypto::overhead(security_level level, const std::string &username) {
  switch (level) {
    case security_level::sign:
      return sign_part_size + username.size();
    case security_level::encrypt:
      return encr_part_size + username.size();
    default:
      return 0;
  }
}

#ifdef USE_SSL

bool collectd::crypto::available() { return true; }

bool collectd::crypto::sign_packet(const std::string &packet, const std::string &username, const std::string &password, std::string &out, std::string &error) {
  if (username.empty() || username.size() > max_username_length) {
    error = "invalid user name length for signed collectd packet";
    return false;
  }
  // The authenticated data is the username immediately followed by the
  // packet — exactly the bytes that end up on the wire after the hash.
  std::string authenticated;
  authenticated.reserve(username.size() + packet.size());
  authenticated.append(username);
  authenticated.append(packet);

  unsigned char mac[EVP_MAX_MD_SIZE];
  unsigned int mac_len = 0;
  if (HMAC(EVP_sha256(), password.data(), static_cast<int>(password.size()), reinterpret_cast<const unsigned char *>(authenticated.data()),
           authenticated.size(), mac, &mac_len) == nullptr ||
      mac_len != 32) {
    error = "HMAC-SHA-256 failed";
    return false;
  }

  out.clear();
  out.reserve(sign_part_size + username.size() + packet.size());
  packet::append_be<uint16_t>(out, static_cast<uint16_t>(part_sign_sha256));
  packet::append_be<uint16_t>(out, static_cast<uint16_t>(sign_part_size + username.size()));
  out.append(reinterpret_cast<const char *>(mac), 32);
  out.append(authenticated);
  return true;
}

bool collectd::crypto::encrypt_packet(const std::string &packet, const std::string &username, const std::string &password, std::string &out,
                                      std::string &error) {
  if (username.empty() || username.size() > max_username_length) {
    error = "invalid user name length for encrypted collectd packet";
    return false;
  }
  const std::size_t total = encr_part_size + username.size() + packet.size();
  if (total > 0xffff) {
    error = "packet too large to encrypt";
    return false;
  }

  // Key: SHA-256 of the password (raw digest, not hex).
  unsigned char key[EVP_MAX_MD_SIZE];
  unsigned int key_len = 0;
  if (EVP_Digest(password.data(), password.size(), key, &key_len, EVP_sha256(), nullptr) != 1 || key_len != 32) {
    error = "SHA-256 key derivation failed";
    return false;
  }

  unsigned char iv[16];
  if (RAND_bytes(iv, sizeof(iv)) != 1) {
    error = "random IV generation failed";
    return false;
  }

  // The encrypted region is SHA-1(packet) followed by the packet.
  unsigned char checksum[EVP_MAX_MD_SIZE];
  unsigned int checksum_len = 0;
  if (EVP_Digest(packet.data(), packet.size(), checksum, &checksum_len, EVP_sha1(), nullptr) != 1 || checksum_len != 20) {
    error = "SHA-1 checksum failed";
    return false;
  }
  std::string plaintext;
  plaintext.reserve(20 + packet.size());
  plaintext.append(reinterpret_cast<const char *>(checksum), 20);
  plaintext.append(packet);

  std::string ciphertext(plaintext.size(), '\0');
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    error = "failed to allocate cipher context";
    return false;
  }
  int cipher_len = 0, final_len = 0;
  const bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_ofb(), nullptr, key, iv) == 1 &&
                  EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(&ciphertext[0]), &cipher_len,
                                    reinterpret_cast<const unsigned char *>(plaintext.data()), static_cast<int>(plaintext.size())) == 1 &&
                  EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(&ciphertext[0]) + cipher_len, &final_len) == 1;
  EVP_CIPHER_CTX_free(ctx);
  if (!ok || static_cast<std::size_t>(cipher_len) + static_cast<std::size_t>(final_len) != plaintext.size()) {
    error = "AES-256/OFB encryption failed";
    return false;
  }

  out.clear();
  out.reserve(total);
  packet::append_be<uint16_t>(out, static_cast<uint16_t>(part_encr_aes256));
  packet::append_be<uint16_t>(out, static_cast<uint16_t>(total));
  packet::append_be<uint16_t>(out, static_cast<uint16_t>(username.size()));
  out.append(username);
  out.append(reinterpret_cast<const char *>(iv), sizeof(iv));
  out.append(ciphertext);
  return true;
}

#else  // !USE_SSL

bool collectd::crypto::available() { return false; }

namespace {
bool no_ssl(std::string &error) {
  error = "NSClient++ was built without OpenSSL: the collectd sign/encrypt security levels are not available";
  return false;
}
}  // namespace

bool collectd::crypto::sign_packet(const std::string &, const std::string &, const std::string &, std::string &, std::string &error) { return no_ssl(error); }

bool collectd::crypto::encrypt_packet(const std::string &, const std::string &, const std::string &, std::string &, std::string &error) {
  return no_ssl(error);
}

#endif
