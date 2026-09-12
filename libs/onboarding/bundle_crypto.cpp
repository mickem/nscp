// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <bytes/base64.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <onboarding/bundle_crypto.hpp>
#include <onboarding/onboarding.hpp>

namespace {

const char magic[] = "NSEB1";
const std::size_t magic_len = 5;
const std::size_t fingerprint_len = 8;
const std::size_t nonce_len = 12;
const std::size_t tag_len = 16;
const std::size_t header_len = magic_len + fingerprint_len + nonce_len;

struct cipher_ctx_deleter {
  void operator()(EVP_CIPHER_CTX *ctx) const { EVP_CIPHER_CTX_free(ctx); }
};
typedef std::unique_ptr<EVP_CIPHER_CTX, cipher_ctx_deleter> cipher_ctx_ptr;

std::string sha256_raw(const std::string &bytes) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  unsigned int len = 0;
  const std::unique_ptr<EVP_MD_CTX, void (*)(EVP_MD_CTX *)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 || EVP_DigestUpdate(ctx.get(), bytes.data(), bytes.size()) != 1 ||
      EVP_DigestFinal_ex(ctx.get(), digest, &len) != 1) {
    throw onboarding::onboarding_error("SHA-256 failed", false);
  }
  return std::string(reinterpret_cast<const char *>(digest), len);
}

std::string to_hex(const std::string &bytes) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const unsigned char c : bytes) {
    out.push_back(digits[c >> 4]);
    out.push_back(digits[c & 0x0f]);
  }
  return out;
}

std::string raw_fingerprint(const std::string &raw_key) { return sha256_raw(raw_key).substr(0, fingerprint_len); }

// name || 0x00 || version: the separator is what stops "secrets1"/".0.0" and
// "secrets"/"1.0.0" from authenticating the same way.
std::string aad_for(const std::string &name, const std::string &version) {
  std::string aad;
  aad.reserve(name.size() + 1 + version.size());
  aad += name;
  aad.push_back('\0');
  aad += version;
  return aad;
}

}  // namespace

std::vector<std::string> onboarding::split_bundle_keys(const std::string &list) {
  std::vector<std::string> keys;
  std::string current;
  for (const char c : list) {
    if (c == ',' || c == ';' || std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) keys.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) keys.push_back(current);
  return keys;
}

bool onboarding::parse_bundle_key(const std::string &key_b64, std::string &raw_key, std::string &error) {
  std::string trimmed = key_b64;
  trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](const unsigned char c) { return !std::isspace(c); }));
  trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](const unsigned char c) { return !std::isspace(c); }).base(), trimmed.end());
  if (trimmed.empty()) {
    error = "bundle key is empty";
    return false;
  }
  // The decoder tolerates junk by skipping it; a key is short and fixed
  // enough that anything outside the alphabet is a paste error worth naming.
  for (const char c : trimmed) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '/' && c != '=') {
      error = "bundle key is not base64";
      return false;
    }
  }
  const std::size_t max_len = b64::b64_decode(nullptr, trimmed.size(), nullptr, 0);
  if (max_len == 0) {
    error = "bundle key is not base64";
    return false;
  }
  std::string decoded(max_len, '\0');
  const std::size_t len = b64::b64_decode(trimmed.data(), trimmed.size(), &decoded[0], decoded.size());
  if (len != bundle_key_bytes) {
    error = "bundle key must decode to 32 bytes";
    return false;
  }
  decoded.resize(len);
  raw_key.swap(decoded);
  return true;
}

std::string onboarding::bundle_key_fingerprint(const std::string &raw_key) { return to_hex(raw_fingerprint(raw_key)); }

bool onboarding::is_encrypted_bundle(const std::string &bytes) { return bytes.size() >= magic_len && bytes.compare(0, magic_len, magic, magic_len) == 0; }

std::string onboarding::encrypted_bundle_fingerprint(const std::string &bytes) {
  if (!is_encrypted_bundle(bytes) || bytes.size() < header_len + tag_len) return "";
  return to_hex(bytes.substr(magic_len, fingerprint_len));
}

onboarding::unseal_status onboarding::decrypt_bundle(const std::string &raw_key, const std::string &name, const std::string &version, const std::string &blob,
                                                     std::string &plaintext, std::string &error) {
  if (raw_key.size() != bundle_key_bytes) {
    error = "bundle key must be 32 bytes";
    return unseal_status::failed;
  }
  if (!is_encrypted_bundle(blob)) {
    error = "not an encrypted bundle";
    return unseal_status::not_encrypted;
  }
  if (blob.size() < header_len + tag_len) {
    error = "encrypted bundle is truncated";
    return unseal_status::corrupt;
  }
  if (blob.compare(magic_len, fingerprint_len, raw_fingerprint(raw_key)) != 0) {
    error = "sealed with another key";
    return unseal_status::wrong_key;
  }
  const unsigned char *nonce = reinterpret_cast<const unsigned char *>(blob.data()) + magic_len + fingerprint_len;
  const std::size_t ct_len = blob.size() - header_len - tag_len;
  const unsigned char *ct = reinterpret_cast<const unsigned char *>(blob.data()) + header_len;
  // OpenSSL wants the tag on its own; the envelope (like RustCrypto and
  // WebCrypto) has it appended to the ciphertext.
  std::string tag = blob.substr(blob.size() - tag_len);
  const std::string aad = aad_for(name, version);

  const cipher_ctx_ptr ctx(EVP_CIPHER_CTX_new());
  std::string out(ct_len + 16, '\0');
  int len = 0;
  int total = 0;
  bool ok = ctx && EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce_len), nullptr) == 1 &&
            EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char *>(raw_key.data()), nonce) == 1 &&
            EVP_DecryptUpdate(ctx.get(), nullptr, &len, reinterpret_cast<const unsigned char *>(aad.data()), static_cast<int>(aad.size())) == 1;
  if (ok && ct_len > 0) {
    ok = EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(&out[0]), &len, ct, static_cast<int>(ct_len)) == 1;
    total = len;
  }
  ok = ok && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag_len), &tag[0]) == 1;
  if (ok) {
    len = 0;
    ok = EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(&out[0]) + total, &len) == 1;
    total += len;
  }
  if (!ok) {
    error = "authentication failed: the bundle was tampered with, or is served under another name or version";
    return unseal_status::failed;
  }
  out.resize(static_cast<std::size_t>(total));
  plaintext.swap(out);
  return unseal_status::ok;
}

std::string onboarding::encrypt_bundle(const std::string &raw_key, const std::string &nonce, const std::string &name, const std::string &version,
                                       const std::string &plaintext) {
  if (raw_key.size() != bundle_key_bytes) throw onboarding_error("bundle key must be 32 bytes", false);
  if (nonce.size() != nonce_len) throw onboarding_error("nonce must be 12 bytes", false);
  const std::string aad = aad_for(name, version);
  const cipher_ctx_ptr ctx(EVP_CIPHER_CTX_new());
  std::string out(plaintext.size() + 16, '\0');
  std::string tag(tag_len, '\0');
  int len = 0;
  int total = 0;
  bool ok = ctx && EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce_len), nullptr) == 1 &&
            EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char *>(raw_key.data()),
                               reinterpret_cast<const unsigned char *>(nonce.data())) == 1 &&
            EVP_EncryptUpdate(ctx.get(), nullptr, &len, reinterpret_cast<const unsigned char *>(aad.data()), static_cast<int>(aad.size())) == 1;
  if (ok && !plaintext.empty()) {
    ok = EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(&out[0]), &len, reinterpret_cast<const unsigned char *>(plaintext.data()),
                           static_cast<int>(plaintext.size())) == 1;
    total = len;
  }
  if (ok) {
    len = 0;
    ok = EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(&out[0]) + total, &len) == 1;
    total += len;
  }
  ok = ok && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag_len), &tag[0]) == 1;
  if (!ok) throw onboarding_error("AES-256-GCM encryption failed", false);
  out.resize(static_cast<std::size_t>(total));
  std::string blob;
  blob.reserve(header_len + out.size() + tag_len);
  blob.append(magic, magic_len);
  blob += raw_fingerprint(raw_key);
  blob += nonce;
  blob += out;
  blob += tag;
  return blob;
}

bool onboarding::open_bundle(const std::vector<std::string> &keys_b64, const std::string &name, const std::string &version, const std::string &bytes,
                             const bool require_encrypted, std::string &plaintext, std::string &error) {
  if (!is_encrypted_bundle(bytes)) {
    if (require_encrypted) {
      error = "bundle is not encrypted but this host requires encrypted bundles";
      return false;
    }
    plaintext = bytes;
    return true;
  }
  const std::string sealed_with = encrypted_bundle_fingerprint(bytes);
  if (sealed_with.empty()) {
    error = "encrypted bundle is truncated";
    return false;
  }
  if (keys_b64.empty()) {
    error = "bundle is encrypted (key " + sealed_with + ") but this host has no bundle key: add it with `nscp enroll --bundle-key`";
    return false;
  }
  std::size_t index = 0;
  for (const std::string &key_b64 : keys_b64) {
    ++index;
    std::string raw_key;
    std::string key_error;
    if (!parse_bundle_key(key_b64, raw_key, key_error)) {
      // A key that cannot even be decoded is a configuration error, not a
      // non-matching key: say so rather than reporting "no key matches".
      error = "bundle key " + std::to_string(index) + " in the enrollment manifest is invalid: " + key_error;
      return false;
    }
    std::string decrypt_error;
    const unseal_status status = decrypt_bundle(raw_key, name, version, bytes, plaintext, decrypt_error);
    if (status == unseal_status::ok) return true;
    if (status == unseal_status::wrong_key) continue;
    error = "encrypted bundle could not be opened: " + decrypt_error;
    return false;
  }
  error = "no configured bundle key matches this bundle (sealed with key " + sealed_with + ")";
  return false;
}
