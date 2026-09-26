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

#include "digest.hpp"

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

using onboarding::detail::sha256_raw;
using onboarding::detail::to_hex;

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

// The version prefix. Present so that a future change to the descriptor's shape
// is a verification failure rather than a silent reinterpretation.
const char kBundleSigDomain[] = "nsclient-fleet/bundle-sig/v2";

namespace {
// Plain decimal, independent of the global locale. str::xtos would do this
// through a stringstream, which is imbued with the global locale: were that
// ever a locale with digit grouping, the tenant id would render as "1,234"
// and every signature on the host would stop verifying. Accumulate through
// the unsigned type so LLONG_MIN, which cannot be negated, still renders.
std::string decimal(const long long value) {
  const bool negative = value < 0;
  unsigned long long magnitude = negative ? 0ULL - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
  std::string digits;
  do {
    digits.push_back(static_cast<char>('0' + (magnitude % 10)));
    magnitude /= 10;
  } while (magnitude != 0);
  if (negative) digits.push_back('-');
  std::reverse(digits.begin(), digits.end());
  return digits;
}
}  // namespace

std::string onboarding::bundle_descriptor::signing_bytes() const {
  std::string out(kBundleSigDomain);
  const std::string fields[] = {decimal(tenant_id), bundle_id, name, version, format, sha256_hex};
  for (const std::string &field : fields) {
    out.push_back('\0');
    out += field;
  }
  return out;
}

onboarding::bundle_descriptor onboarding::describe_bundle(const long long tenant_id, const bundle_info &bundle) {
  bundle_descriptor descriptor;
  descriptor.tenant_id = tenant_id;
  descriptor.bundle_id = bundle.id;
  descriptor.name = bundle.name;
  descriptor.version = bundle.version;
  descriptor.format = bundle.format;
  descriptor.sha256_hex = bundle.sha256;
  return descriptor;
}

bool onboarding::verify_ed25519(const std::string &pub_pem, const std::string &message, const std::string &signature_b64, std::string &error) {
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
    error = "invalid signing public key";
    return false;
  }
  const std::unique_ptr<EVP_MD_CTX, evp_md_ctx_deleter> ctx(EVP_MD_CTX_new());
  // Ed25519 is a one-shot algorithm that hashes internally, so the message is
  // verified directly rather than being digested first.
  if (!ctx || EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) != 1) {
    error = "failed to initialize signature verification";
    return false;
  }
  if (EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char *>(signature.data()), signature.size(),
                       reinterpret_cast<const unsigned char *>(message.data()), message.size()) != 1) {
    error = "signature verification failed";
    return false;
  }
  return true;
}

bool onboarding::same_public_key(const std::string &left_pem, const std::string &right_pem) {
  if (left_pem == right_pem) return true;
  const auto read = [](const std::string &pem) {
    const std::unique_ptr<BIO, bio_deleter> bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    return std::unique_ptr<EVP_PKEY, evp_pkey_deleter>(bio ? PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr) : nullptr);
  };
  const std::unique_ptr<EVP_PKEY, evp_pkey_deleter> left = read(left_pem);
  const std::unique_ptr<EVP_PKEY, evp_pkey_deleter> right = read(right_pem);
  // One of them is not a key this process can decode - an empty state field, a
  // truncated PEM, a placeholder. Nothing to compare but the text, which the
  // equality above already answered.
  if (!left || !right) return false;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return EVP_PKEY_eq(left.get(), right.get()) == 1;
#else
  return EVP_PKEY_cmp(left.get(), right.get()) == 1;
#endif
}

bool onboarding::verify_bundle(const std::string &pub_pem, const std::string &bytes, const bundle_descriptor &descriptor,
                               const std::string &signature_b64, std::string &error) {
  const std::string digest = sha256_raw(bytes);
  if (to_hex(digest) != to_lower(descriptor.sha256_hex)) {
    error = "checksum mismatch: expected " + descriptor.sha256_hex + " got " + to_hex(digest);
    return false;
  }
  return verify_ed25519(pub_pem, descriptor.signing_bytes(), signature_b64, error);
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
  // Round down rather than toward zero. ASN1_TIME_diff gives days and seconds
  // with a common sign, so a certificate that expired an hour ago is days=0,
  // seconds=-3600 - returning days as-is would report 0, the same value as
  // "still valid, expires within the day". Flooring keeps negative meaning
  // expired. (Renewal here triggers on `< renew_threshold_days`, so 0 already
  // renewed; the sign still has to be right for anything else reading this.)
  if (seconds < 0) days -= 1;
  return days;
}
