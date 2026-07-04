/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

// Cross-platform certificate parsing via OpenSSL. This file is compiled on both
// Windows and Unix; the Windows store enumerator (cert_source_win.cpp) reuses
// parse_der() here so there is a single cert-parsing code path.

#include "cert_source.hpp"

#include <ctime>

#include <boost/filesystem.hpp>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

namespace cert_source {

// RAII wrappers for the OpenSSL C handles used below (same idiom as
// include/net/socket/socket_helpers.cpp). from_x509() takes a *borrowed* X509*
// and never frees it, so it stays a raw non-owning parameter.
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;
using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using BIGNUM_ptr = std::unique_ptr<BIGNUM, decltype(&::BN_free)>;
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

// Build a filter object from an already-parsed X509 (does not take ownership).
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
  return obj;
}

}  // namespace

cert_filter::filter_obj_ptr parse_der(const unsigned char *der, size_t len, const std::string &source, const std::string &store, std::string &error) {
  const unsigned char *p = der;
  X509_ptr x(d2i_X509(nullptr, &p, static_cast<long>(len)), X509_free);
  if (!x) {
    error = "failed to parse DER certificate";
    return nullptr;
  }
  return from_x509(x.get(), source, store);
}

namespace {

// Parse one file, appending every certificate it contains. Tries PEM first (a
// bundle may hold several), then falls back to single-certificate DER.
void load_one_file(const std::string &path, std::vector<cert_filter::filter_obj_ptr> &out, std::vector<std::string> &errors) {
  BIO_ptr bio(BIO_new_file(path.c_str(), "rb"), BIO_free);
  if (!bio) {
    errors.push_back("cannot open " + path);
    return;
  }

  size_t found = 0;
  for (;;) {
    X509_ptr x(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free);
    if (!x) break;
    out.push_back(from_x509(x.get(), path, "file"));
    ++found;
  }

  if (found == 0) {
    // Not PEM (or empty) — rewind and try DER.
    BIO_reset(bio.get());
    X509_ptr x(d2i_X509_bio(bio.get(), nullptr), X509_free);
    if (x) {
      out.push_back(from_x509(x.get(), path, "file"));
      ++found;
    }
  }

  if (found == 0) errors.push_back("no certificate found in " + path);
}

}  // namespace

void load_files(const std::vector<std::string> &paths, bool recurse, std::vector<cert_filter::filter_obj_ptr> &out, std::vector<std::string> &errors) {
  namespace fs = boost::filesystem;
  for (const std::string &p : paths) {
    boost::system::error_code ec;
    fs::path path(p);
    if (fs::is_directory(path, ec)) {
      if (recurse) {
        for (fs::recursive_directory_iterator it(path, ec), end; it != end && !ec; it.increment(ec)) {
          if (fs::is_regular_file(it->path(), ec)) load_one_file(it->path().string(), out, errors);
        }
      } else {
        for (fs::directory_iterator it(path, ec), end; it != end && !ec; it.increment(ec)) {
          if (fs::is_regular_file(it->path(), ec)) load_one_file(it->path().string(), out, errors);
        }
      }
    } else if (fs::is_regular_file(path, ec)) {
      load_one_file(p, out, errors);
    } else {
      errors.push_back("not found: " + p);
    }
  }
}

}  // namespace cert_source
