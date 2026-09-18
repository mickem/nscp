// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_crypt.hpp"

#include <bytes/base64.h>
#include <openssl/evp.h>

#include <cctype>
#include <memory>
#include <vector>

namespace gearman {

namespace {

struct cipher_ctx_deleter {
  void operator()(EVP_CIPHER_CTX *ctx) const { EVP_CIPHER_CTX_free(ctx); }
};
typedef std::unique_ptr<EVP_CIPHER_CTX, cipher_ctx_deleter> cipher_ctx_ptr;

cipher_ctx_ptr make_context(const std::string &password, const bool encrypt) {
  cipher_ctx_ptr ctx(EVP_CIPHER_CTX_new());
  if (!ctx) throw crypt_error("failed to allocate a cipher context");
  const std::string key = make_key(password);
  const unsigned char *key_bytes_ptr = reinterpret_cast<const unsigned char *>(key.data());
  const int ok = encrypt ? EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr, key_bytes_ptr, nullptr)
                         : EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr, key_bytes_ptr, nullptr);
  if (ok != 1) throw crypt_error("failed to initialise AES-256-ECB");
  // The padding is applied by hand above (and by the cores), so OpenSSL must
  // not add or verify a PKCS#7 pad of its own.
  EVP_CIPHER_CTX_set_padding(ctx.get(), 0);
  return ctx;
}

std::string strip_whitespace(const std::string &input) {
  std::string out;
  out.reserve(input.size());
  for (const char c : input) {
    if (!std::isspace(static_cast<unsigned char>(c))) out.push_back(c);
  }
  return out;
}

std::string base64_decode(const std::string &encoded) {
  const std::string packed = strip_whitespace(encoded);
  if (packed.empty()) throw crypt_error("gearman payload is empty");
  const std::size_t max_len = b64::b64_decode(nullptr, packed.size(), nullptr, 0);
  if (max_len == 0) throw crypt_error("gearman payload is not base64");
  std::string decoded(max_len, '\0');
  const std::size_t len = b64::b64_decode(packed.data(), packed.size(), &decoded[0], decoded.size());
  if (len == 0) throw crypt_error("gearman payload is not base64");
  decoded.resize(len);
  return decoded;
}

std::string base64_encode(const std::string &raw) {
  if (raw.empty()) return std::string();
  const std::size_t len = b64::b64_encode(raw.data(), raw.size(), nullptr, 0);
  if (len == 0) throw crypt_error("failed to base64 encode the gearman payload");
  std::string out(len, '\0');
  b64::b64_encode(raw.data(), raw.size(), &out[0], out.size());
  return out;
}

}  // namespace

std::string make_key(const std::string &password) {
  std::string key(key_bytes, '\0');
  const std::size_t len = password.size() < key_bytes ? password.size() : key_bytes;
  key.replace(0, len, password, 0, len);
  return key;
}

std::string encrypt_payload(const std::string &text, const std::string &password) {
  std::string plain = text;
  plain.push_back('\0');
  if (block_size % plain.size() != 0) plain.append(block_size - (plain.size() % block_size), '\0');
  // Only reachable for the four lengths the reference's own rule leaves short.
  if (plain.size() % block_size != 0) plain.append(block_size - (plain.size() % block_size), '\0');

  const cipher_ctx_ptr ctx = make_context(password, true);
  std::vector<unsigned char> out(plain.size() + block_size);
  int written = 0;
  if (EVP_EncryptUpdate(ctx.get(), out.data(), &written, reinterpret_cast<const unsigned char *>(plain.data()), static_cast<int>(plain.size())) != 1) {
    throw crypt_error("failed to encrypt the gearman payload");
  }
  int final_written = 0;
  if (EVP_EncryptFinal_ex(ctx.get(), out.data() + written, &final_written) != 1) throw crypt_error("failed to encrypt the gearman payload");
  return base64_encode(std::string(reinterpret_cast<const char *>(out.data()), static_cast<std::size_t>(written + final_written)));
}

std::string decrypt_payload(const std::string &encoded, const std::string &password) {
  const std::string raw = base64_decode(encoded);
  const std::size_t usable = raw.size() - (raw.size() % block_size);
  if (usable == 0) throw crypt_error("gearman payload is shorter than one AES block");

  const cipher_ctx_ptr ctx = make_context(password, false);
  std::vector<unsigned char> out(usable + block_size);
  int written = 0;
  if (EVP_DecryptUpdate(ctx.get(), out.data(), &written, reinterpret_cast<const unsigned char *>(raw.data()), static_cast<int>(usable)) != 1) {
    throw crypt_error("failed to decrypt the gearman payload");
  }
  int final_written = 0;
  if (EVP_DecryptFinal_ex(ctx.get(), out.data() + written, &final_written) != 1) throw crypt_error("failed to decrypt the gearman payload");

  const std::string plain(reinterpret_cast<const char *>(out.data()), static_cast<std::size_t>(written + final_written));
  const std::string::size_type end = plain.find('\0');
  return end == std::string::npos ? plain : plain.substr(0, end);
}

std::string encode_plain_payload(const std::string &text) { return base64_encode(text); }

std::string decode_plain_payload(const std::string &encoded) { return base64_decode(encoded); }

std::string encode_payload(const std::string &text, const envelope &settings) {
  return settings.encryption ? encrypt_payload(text, settings.key) : encode_plain_payload(text);
}

std::string decode_payload(const std::string &encoded, const envelope &settings) {
  return settings.encryption ? decrypt_payload(encoded, settings.key) : decode_plain_payload(encoded);
}

}  // namespace gearman
