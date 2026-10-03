// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "cert_loader.h"

#include <fstream>
#include <memory>
#include <nsclient/nsclient_exception.hpp>
#include <sstream>

#ifdef NSCP_CERT_LOADER_OPENSSL
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#endif

namespace Mongoose {
namespace cert_loader {

std::string load_file(const std::string& path, const std::string& hint) {
  // A default std::ifstream doesn't throw, so a missing/unreadable file would
  // otherwise yield an empty string silently — making a TLS misconfiguration
  // look like an empty certificate much later. Detect open/read failures and
  // report them so the cause is obvious.
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw nsclient::nsclient_exception("Failed to open " + hint + " file: " + path);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  if (file.bad()) {
    throw nsclient::nsclient_exception("Failed to read " + hint + " file: " + path);
  }
  return buffer.str();
}

namespace {
#ifdef NSCP_CERT_LOADER_OPENSSL
// Reading the files is not enough to say TLS will work: a file holding no
// certificate, or no key, or a key for another certificate, reads fine and then
// fails in the first handshake (mongoose) or at start() (beast). A caller that
// must not fall back to cleartext decides on setSsl()'s answer, so the answer
// has to mean "this certificate and key can be served".
void validate(const std::string& cert, const std::string& key, const std::string& cert_path) {
  const std::unique_ptr<BIO, decltype(&BIO_free)> cert_bio(BIO_new_mem_buf(cert.data(), static_cast<int>(cert.size())), &BIO_free);
  const std::unique_ptr<X509, decltype(&X509_free)> x509(cert_bio ? PEM_read_bio_X509(cert_bio.get(), nullptr, nullptr, nullptr) : nullptr, &X509_free);
  if (!x509) {
    throw nsclient::nsclient_exception("No PEM certificate found in " + cert_path);
  }
  const std::unique_ptr<BIO, decltype(&BIO_free)> key_bio(BIO_new_mem_buf(key.data(), static_cast<int>(key.size())), &BIO_free);
  const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(key_bio ? PEM_read_bio_PrivateKey(key_bio.get(), nullptr, nullptr, nullptr) : nullptr,
                                                                 &EVP_PKEY_free);
  if (!pkey) {
    throw nsclient::nsclient_exception("No PEM private key found for the certificate " + cert_path);
  }
  if (X509_check_private_key(x509.get(), pkey.get()) != 1) {
    throw nsclient::nsclient_exception("The private key does not match the certificate " + cert_path);
  }
}
#else
void validate(const std::string&, const std::string&, const std::string&) {}
#endif
}  // namespace

std::pair<std::string, std::string> load_certificates(const std::string& cert_path, const std::string& key_path) {
  auto cert = load_file(cert_path, "certificate");
  auto key = key_path.empty() ? cert : load_file(key_path, "private key");
  validate(cert, key, cert_path);
  return {cert, key};
}

}  // namespace cert_loader
}  // namespace Mongoose
