// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Cross-platform certificate parsing via OpenSSL. This file is compiled on both
// Windows and Unix; the Windows store enumerator (cert_source_win.cpp) collects
// DER blobs and hands them to finalize_der_batch() here so there is a single
// cert-parsing/trust code path.

#include "cert_source.hpp"

#include <ctime>

#include <boost/filesystem.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

namespace cert_source {

// RAII wrappers for the OpenSSL C handles used below (same idiom as
// include/net/socket/socket_helpers.cpp). from_x509() takes a *borrowed* X509*
// and never frees it, so it stays a raw non-owning parameter.
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;
using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using BIGNUM_ptr = std::unique_ptr<BIGNUM, decltype(&::BN_free)>;
using X509_STORE_ptr = std::unique_ptr<X509_STORE, decltype(&::X509_STORE_free)>;
using X509_STORE_CTX_ptr = std::unique_ptr<X509_STORE_CTX, decltype(&::X509_STORE_CTX_free)>;
struct openssl_free_deleter {
  void operator()(void *p) const { OPENSSL_free(p); }
};
using openssl_str = std::unique_ptr<char, openssl_free_deleter>;

namespace {

std::string name_to_string(X509_NAME *name) {
  if (name == nullptr) return "";
  char buf[1024] = {0};
  X509_NAME_oneline(name, buf, sizeof(buf) - 1);
  return std::string(buf);
}

// Convert an ASN1 time (UTCTime / GeneralizedTime) to unix epoch seconds.
long long asn1_to_epoch(const ASN1_TIME *t) {
  if (t == nullptr) return 0;
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  if (ASN1_TIME_to_tm(t, &tm) != 1) return 0;
#ifdef WIN32
  return static_cast<long long>(_mkgmtime(&tm));
#else
  return static_cast<long long>(timegm(&tm));
#endif
}

std::string sha1_fingerprint(X509 *x) {
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n = 0;
  if (X509_digest(x, EVP_sha1(), md, &n) != 1) return "";
  static const char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(n * 2);
  for (unsigned int i = 0; i < n; ++i) {
    out.push_back(hex[(md[i] >> 4) & 0xf]);
    out.push_back(hex[md[i] & 0xf]);
  }
  return out;
}

std::string serial_string(X509 *x) {
  ASN1_INTEGER *sn = X509_get_serialNumber(x);  // internal pointer, not owned
  if (sn == nullptr) return "";
  BIGNUM_ptr bn(ASN1_INTEGER_to_BN(sn, nullptr), BN_free);
  if (!bn) return "";
  openssl_str hex(BN_bn2hex(bn.get()));
  return hex ? std::string(hex.get()) : "";
}

bool is_weak_signature(int nid) {
  switch (nid) {
    case NID_md2WithRSAEncryption:
    case NID_md4WithRSAEncryption:
    case NID_md5WithRSAEncryption:
    case NID_sha1WithRSAEncryption:
    case NID_dsaWithSHA1:
    case NID_ecdsa_with_SHA1:
      return true;
    default:
      return false;
  }
}

// Fill the signature/key/self-signed security fields from a single certificate.
void fill_security_fields(X509 *x, cert_filter::filter_obj &o) {
  const int sig_nid = X509_get_signature_nid(x);
  const char *sig_ln = OBJ_nid2ln(sig_nid);
  o.signature_algorithm = sig_ln != nullptr ? sig_ln : "unknown";
  o.weak_signature = is_weak_signature(sig_nid) ? 1 : 0;

  EVP_PKEY *pk = X509_get0_pubkey(x);  // borrowed, not owned
  if (pk != nullptr) {
    o.key_size = EVP_PKEY_bits(pk);
    switch (EVP_PKEY_base_id(pk)) {
      case EVP_PKEY_RSA:
        o.key_type = "RSA";
        o.weak_key = o.key_size < 2048 ? 1 : 0;
        break;
      case EVP_PKEY_DSA:
        o.key_type = "DSA";
        o.weak_key = o.key_size < 2048 ? 1 : 0;
        break;
      case EVP_PKEY_EC:
        o.key_type = "EC";
        o.weak_key = o.key_size < 256 ? 1 : 0;
        break;
      default: {
        const char *sn = OBJ_nid2sn(EVP_PKEY_base_id(pk));
        o.key_type = sn != nullptr ? sn : "unknown";
        o.weak_key = 0;
        break;
      }
    }
  }

  o.self_signed = (X509_check_issued(x, x) == X509_V_OK) ? 1 : 0;
}

// Build a filter object from an already-parsed X509 (does not take ownership).
// Trust is set separately by the caller (it needs the CA store + intermediates).
cert_filter::filter_obj_ptr from_x509(X509 *x, const std::string &source, const std::string &store) {
  auto obj = std::make_shared<cert_filter::filter_obj>();
  obj->subject = name_to_string(X509_get_subject_name(x));
  obj->issuer = name_to_string(X509_get_issuer_name(x));
  obj->thumbprint = sha1_fingerprint(x);
  obj->serial = serial_string(x);
  obj->source = source;
  obj->store = store;
  obj->not_before = asn1_to_epoch(X509_get0_notBefore(x));
  obj->not_after = asn1_to_epoch(X509_get0_notAfter(x));
  fill_security_fields(x, *obj);
  return obj;
}

X509_STORE_ptr build_trust_store(const std::string &ca_file) {
  X509_STORE_ptr store(X509_STORE_new(), X509_STORE_free);
  if (!store) return store;
  if (!ca_file.empty()) {
    X509_STORE_load_locations(store.get(), ca_file.c_str(), nullptr);
  } else {
    X509_STORE_set_default_paths(store.get());
  }
  return store;
}

// Does `leaf` chain to a trusted root, using `intermediates` as candidate
// non-trusted CAs? Time validity is intentionally ignored so `trusted` stays
// orthogonal to the `expired`/`not_yet_valid` keywords.
bool verify_trust(X509 *leaf, STACK_OF(X509) * intermediates, X509_STORE *store) {
  if (store == nullptr) return false;
  X509_STORE_CTX_ptr ctx(X509_STORE_CTX_new(), X509_STORE_CTX_free);
  if (!ctx) return false;
  if (X509_STORE_CTX_init(ctx.get(), store, leaf, intermediates) != 1) return false;
  X509_VERIFY_PARAM *param = X509_STORE_CTX_get0_param(ctx.get());
  if (param != nullptr) X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_NO_CHECK_TIME);
  return X509_verify_cert(ctx.get()) == 1;
}

// Shared tail for both sources: given owned X509s (parallel source/store labels),
// compute trust for each against the whole set and append filter objects.
void finalize_x509(std::vector<X509_ptr> &certs, const std::vector<std::string> &sources, const std::vector<std::string> &stores,
                   const std::string &ca_file, std::vector<cert_filter::filter_obj_ptr> &out) {
  X509_STORE_ptr store = build_trust_store(ca_file);

  STACK_OF(X509) *intermediates = sk_X509_new_null();
  if (intermediates != nullptr) {
    for (const X509_ptr &c : certs) sk_X509_push(intermediates, c.get());
  }

  for (size_t i = 0; i < certs.size(); ++i) {
    cert_filter::filter_obj_ptr obj = from_x509(certs[i].get(), sources[i], stores[i]);
    obj->trusted = verify_trust(certs[i].get(), intermediates, store.get()) ? 1 : 0;
    out.push_back(obj);
  }

  if (intermediates != nullptr) sk_X509_free(intermediates);  // frees the stack, not the certs
}

// Read every certificate in one file into `certs` (owned). Tries PEM (a bundle
// may hold several), then single-cert DER, then PKCS#12 (.pfx) with `password`.
void read_certs_from_file(const std::string &path, const std::string &password, std::vector<X509_ptr> &certs, std::vector<std::string> &sources,
                          std::vector<std::string> &errors) {
  BIO_ptr bio(BIO_new_file(path.c_str(), "rb"), BIO_free);
  if (!bio) {
    errors.push_back("cannot open " + path);
    return;
  }

  size_t found = 0;
  for (;;) {
    X509_ptr x(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free);
    if (!x) break;
    certs.push_back(std::move(x));
    sources.push_back(path);
    ++found;
  }

  if (found == 0) {
    BIO_reset(bio.get());
    X509_ptr x(d2i_X509_bio(bio.get(), nullptr), X509_free);
    if (x) {
      certs.push_back(std::move(x));
      sources.push_back(path);
      ++found;
    }
  }

  if (found == 0) {
    // Last resort: a PKCS#12 (.pfx/.p12) container.
    BIO_reset(bio.get());
    PKCS12 *p12 = d2i_PKCS12_bio(bio.get(), nullptr);
    if (p12 != nullptr) {
      EVP_PKEY *pkey = nullptr;
      X509 *cert = nullptr;
      STACK_OF(X509) *ca = nullptr;
      if (PKCS12_parse(p12, password.c_str(), &pkey, &cert, &ca) == 1) {
        if (cert != nullptr) {
          certs.emplace_back(cert, X509_free);
          sources.push_back(path);
          ++found;
        }
        if (ca != nullptr) {
          for (int i = 0; i < sk_X509_num(ca); ++i) {
            certs.emplace_back(sk_X509_value(ca, i), X509_free);  // adopt each cert
            sources.push_back(path);
            ++found;
          }
          sk_X509_free(ca);  // stack only; certs adopted above
        }
        if (pkey != nullptr) EVP_PKEY_free(pkey);
      }
      PKCS12_free(p12);
    }
  }

  if (found == 0) errors.push_back("no certificate found in " + path);
}

}  // namespace

void finalize_der_batch(const std::vector<der_cert> &ders, const std::string &ca_file, std::vector<cert_filter::filter_obj_ptr> &out,
                        std::vector<std::string> &errors) {
  std::vector<X509_ptr> certs;
  std::vector<std::string> sources;
  std::vector<std::string> stores;
  for (const der_cert &d : ders) {
    const unsigned char *p = d.der.data();
    X509_ptr x(d2i_X509(nullptr, &p, static_cast<long>(d.der.size())), X509_free);
    if (!x) {
      errors.push_back("failed to parse certificate from " + d.source);
      continue;
    }
    certs.push_back(std::move(x));
    sources.push_back(d.source);
    stores.push_back(d.store);
  }
  finalize_x509(certs, sources, stores, ca_file, out);
}

void load_files(const std::vector<std::string> &paths, bool recurse, const std::string &ca_file, const std::string &password,
                std::vector<cert_filter::filter_obj_ptr> &out, std::vector<std::string> &errors) {
  namespace fs = boost::filesystem;

  std::vector<X509_ptr> certs;
  std::vector<std::string> sources;

  const auto handle = [&](const std::string &f) { read_certs_from_file(f, password, certs, sources, errors); };

  for (const std::string &p : paths) {
    boost::system::error_code ec;
    fs::path path(p);
    if (fs::is_directory(path, ec)) {
      if (recurse) {
        for (fs::recursive_directory_iterator it(path, ec), end; it != end && !ec; it.increment(ec)) {
          if (fs::is_regular_file(it->path(), ec)) handle(it->path().string());
        }
      } else {
        for (fs::directory_iterator it(path, ec), end; it != end && !ec; it.increment(ec)) {
          if (fs::is_regular_file(it->path(), ec)) handle(it->path().string());
        }
      }
    } else if (fs::is_regular_file(path, ec)) {
      handle(p);
    } else {
      errors.push_back("not found: " + p);
    }
  }

  const std::vector<std::string> stores(sources.size(), "file");
  finalize_x509(certs, sources, stores, ca_file, out);
}

}  // namespace cert_source
