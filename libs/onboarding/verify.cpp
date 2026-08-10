// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <bytes/base64.h>
#include <onboarding/sync.hpp>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <algorithm>
#include <cctype>
#include <memory>

namespace {

struct bio_deleter {
  void operator()(BIO *p) const { BIO_free(p); }
};
struct evp_pkey_deleter {
  void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
struct evp_md_ctx_deleter {
  void operator()(EVP_MD_CTX *p) const { EVP_MD_CTX_free(p); }
};
struct x509_deleter {
  void operator()(X509 *p) const { X509_free(p); }
};

std::string sha256_raw(const std::string &bytes) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  if (EVP_Digest(bytes.data(), bytes.size(), digest, &length, EVP_sha256(), nullptr) != 1) {
    throw onboarding::onboarding_error("SHA-256 digest failed", false);
  }
  return std::string(reinterpret_cast<const char *>(digest), length);
}

std::string to_hex(const std::string &bytes) {
  static const char *digits = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const char c : bytes) {
    const auto b = static_cast<unsigned char>(c);
    out.push_back(digits[b >> 4]);
    out.push_back(digits[b & 0x0f]);
  }
  return out;
}

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

}  // namespace

std::string onboarding::sha256_hex(const std::string &bytes) { return to_hex(sha256_raw(bytes)); }

onboarding::sha256_stream::sha256_stream() : ctx_(EVP_MD_CTX_new()) {
  EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(ctx_);
  if (ctx == nullptr || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    ctx_ = nullptr;
    throw onboarding_error("SHA-256 digest failed", false);
  }
}

onboarding::sha256_stream::~sha256_stream() { EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(ctx_)); }

void onboarding::sha256_stream::update(const std::string &bytes) {
  if (ctx_ == nullptr || EVP_DigestUpdate(static_cast<EVP_MD_CTX *>(ctx_), bytes.data(), bytes.size()) != 1) {
    throw onboarding_error("SHA-256 digest failed", false);
  }
}

std::string onboarding::sha256_stream::hex_final() {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  if (ctx_ == nullptr || EVP_DigestFinal_ex(static_cast<EVP_MD_CTX *>(ctx_), digest, &length) != 1) {
    throw onboarding_error("SHA-256 digest failed", false);
  }
  return to_hex(std::string(reinterpret_cast<const char *>(digest), length));
}

bool onboarding::verify_bundle(const std::string &pub_pem, const std::string &bytes, const std::string &sha256_hex_expected,
                               const std::string &signature_b64, std::string &error) {
  const std::string digest = sha256_raw(bytes);
  if (to_hex(digest) != to_lower(sha256_hex_expected)) {
    error = "checksum mismatch: expected " + sha256_hex_expected + " got " + to_hex(digest);
    return false;
  }

  const std::size_t max_len = b64::b64_decode(nullptr, signature_b64.size(), nullptr, 0);
  if (max_len == 0) {
    error = "invalid base64 signature";
    return false;
  }
  std::string signature(max_len, '\0');
  const std::size_t sig_len = b64::b64_decode(signature_b64.data(), signature_b64.size(), &signature[0], signature.size());
  if (sig_len == 0) {
    error = "invalid base64 signature";
    return false;
  }
  signature.resize(sig_len);

  const std::unique_ptr<BIO, bio_deleter> bio(BIO_new_mem_buf(pub_pem.data(), static_cast<int>(pub_pem.size())));
  const std::unique_ptr<EVP_PKEY, evp_pkey_deleter> key(bio ? PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr) : nullptr);
  if (!key) {
    error = "invalid bundle signing public key";
    return false;
  }
  const std::unique_ptr<EVP_MD_CTX, evp_md_ctx_deleter> ctx(EVP_MD_CTX_new());
  // Ed25519 is a one-shot algorithm (no digest): the signature covers the
  // 32-byte SHA-256 digest of the bundle, per the fleet protocol.
  if (!ctx || EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) != 1) {
    error = "failed to initialize signature verification";
    return false;
  }
  if (EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char *>(signature.data()), signature.size(),
                       reinterpret_cast<const unsigned char *>(digest.data()), digest.size()) != 1) {
    error = "signature verification failed";
    return false;
  }
  return true;
}

long onboarding::days_until_expiry(const std::string &cert_pem) {
  const std::unique_ptr<BIO, bio_deleter> bio(BIO_new_mem_buf(cert_pem.data(), static_cast<int>(cert_pem.size())));
  const std::unique_ptr<X509, x509_deleter> cert(bio ? PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) : nullptr);
  if (!cert) {
    throw onboarding_error("Failed to parse certificate", false);
  }
  int days = 0, seconds = 0;
  if (ASN1_TIME_diff(&days, &seconds, nullptr, X509_get0_notAfter(cert.get())) != 1) {
    throw onboarding_error("Failed to compute certificate expiry", false);
  }
  return days;
}
