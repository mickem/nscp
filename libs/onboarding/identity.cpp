// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <onboarding/onboarding.hpp>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <memory>

namespace {

struct evp_pkey_deleter {
  void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
struct evp_pkey_ctx_deleter {
  void operator()(EVP_PKEY_CTX *p) const { EVP_PKEY_CTX_free(p); }
};
struct x509_req_deleter {
  void operator()(X509_REQ *p) const { X509_REQ_free(p); }
};
struct bio_deleter {
  void operator()(BIO *p) const { BIO_free(p); }
};

typedef std::unique_ptr<EVP_PKEY, evp_pkey_deleter> pkey_ptr;
typedef std::unique_ptr<EVP_PKEY_CTX, evp_pkey_ctx_deleter> pkey_ctx_ptr;
typedef std::unique_ptr<X509_REQ, x509_req_deleter> x509_req_ptr;
typedef std::unique_ptr<BIO, bio_deleter> bio_ptr;

onboarding::onboarding_error openssl_error(const std::string &operation) {
  char buf[256] = {0};
  const unsigned long code = ERR_get_error();
  if (code == 0) {
    return {operation + " failed", false};
  }
  ERR_error_string_n(code, buf, sizeof(buf));
  return {operation + " failed: " + buf, false};
}

std::string bio_to_string(BIO *bio) {
  char *data = nullptr;
  const long len = BIO_get_mem_data(bio, &data);
  if (len <= 0 || data == nullptr) {
    throw openssl_error("Reading PEM buffer");
  }
  return {data, static_cast<std::size_t>(len)};
}

}  // namespace

onboarding::identity onboarding::generate_identity(const std::string &cn) {
  const pkey_ctx_ptr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr));
  if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0) {
    throw openssl_error("Initializing Ed25519 key generation");
  }
  EVP_PKEY *raw_key = nullptr;
  if (EVP_PKEY_keygen(ctx.get(), &raw_key) <= 0) {
    throw openssl_error("Generating Ed25519 key");
  }
  const pkey_ptr pkey(raw_key);

  const x509_req_ptr req(X509_REQ_new());
  if (!req || X509_REQ_set_version(req.get(), 0L) != 1) {
    throw openssl_error("Creating certificate request");
  }
  X509_NAME *name = X509_REQ_get_subject_name(req.get());
  if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>(cn.c_str()), -1, -1, 0) != 1) {
    throw openssl_error("Setting certificate request subject");
  }
  if (X509_REQ_set_pubkey(req.get(), pkey.get()) != 1) {
    throw openssl_error("Setting certificate request public key");
  }
  // Ed25519 is a one-shot signature algorithm: the digest must be NULL.
  if (X509_REQ_sign(req.get(), pkey.get(), nullptr) <= 0) {
    throw openssl_error("Signing certificate request");
  }

  identity result;
  {
    const bio_ptr bio(BIO_new(BIO_s_mem()));
    if (!bio || PEM_write_bio_PrivateKey(bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1) {
      throw openssl_error("Encoding private key");
    }
    result.private_key_pem = bio_to_string(bio.get());
  }
  {
    const bio_ptr bio(BIO_new(BIO_s_mem()));
    if (!bio || PEM_write_bio_X509_REQ(bio.get(), req.get()) != 1) {
      throw openssl_error("Encoding certificate request");
    }
    result.csr_pem = bio_to_string(bio.get());
  }
  return result;
}
