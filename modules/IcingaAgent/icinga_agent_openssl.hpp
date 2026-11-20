// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Owning handles for the OpenSSL objects the module creates, so every early
// return and every throw frees them. The deleters are stateless functors: a
// unique_ptr holding one is the size of a raw pointer, and it can be
// default-constructed empty.

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include <memory>

namespace icinga_agent {
namespace ossl {

struct x509_free {
  void operator()(X509 *p) const { X509_free(p); }
};
struct evp_pkey_free {
  void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
struct bio_free {
  void operator()(BIO *p) const { BIO_free(p); }
};
struct x509_store_free {
  void operator()(X509_STORE *p) const { X509_STORE_free(p); }
};
struct x509_store_ctx_free {
  void operator()(X509_STORE_CTX *p) const { X509_STORE_CTX_free(p); }
};

using x509_ptr = std::unique_ptr<X509, x509_free>;
using evp_pkey_ptr = std::unique_ptr<EVP_PKEY, evp_pkey_free>;
using bio_ptr = std::unique_ptr<BIO, bio_free>;
using x509_store_ptr = std::unique_ptr<X509_STORE, x509_store_free>;
using x509_store_ctx_ptr = std::unique_ptr<X509_STORE_CTX, x509_store_ctx_free>;

}  // namespace ossl
}  // namespace icinga_agent
