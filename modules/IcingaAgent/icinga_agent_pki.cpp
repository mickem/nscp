// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_pki.hpp"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "icinga_agent_openssl.hpp"

namespace icinga_agent {

namespace {

std::string openssl_error() {
  const unsigned long code = ERR_get_error();
  if (code == 0) return "unknown OpenSSL error";
  char buf[256];
  ERR_error_string_n(code, buf, sizeof(buf));
  return buf;
}

std::string bio_to_string(BIO *bio) {
  char *data = nullptr;
  const long len = BIO_get_mem_data(bio, &data);
  return std::string(data, static_cast<std::size_t>(len));
}

void write_file(const std::string &path, const std::string &content, const bool secret) {
  boost::filesystem::create_directories(boost::filesystem::path(path).parent_path());
  if (secret) {
    // Restrict the file while it is still empty, so the key is never on
    // disk under the umask's (usually world-readable) mode.
    {
      std::ofstream create(path, std::ios::binary | std::ios::trunc);
    }
    boost::system::error_code ec;
    boost::filesystem::permissions(path, boost::filesystem::owner_read | boost::filesystem::owner_write, ec);
    if (ec) throw std::runtime_error("Failed to restrict " + path + ": " + ec.message());
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("Failed to write " + path);
  out << content;
  out.close();
  if (!out) throw std::runtime_error("Failed to write " + path);
}

ossl::bio_ptr memory_bio() {
  ossl::bio_ptr bio(BIO_new(BIO_s_mem()));
  if (!bio) throw std::runtime_error("Failed to allocate a buffer: " + openssl_error());
  return bio;
}

// Read through a BIO rather than a FILE*: OpenSSL's FILE* entry points need
// the applink shim on Windows when the CRTs differ, a BIO opened by OpenSSL
// does not.
ossl::x509_ptr load_certificate(const std::string &path) {
  const ossl::bio_ptr bio(BIO_new_file(path.c_str(), "rb"));
  if (!bio) {
    ERR_clear_error();
    return {};
  }
  return ossl::x509_ptr(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
}

ossl::x509_ptr parse_certificate(const std::string &pem, const char *what) {
  if (pem.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::runtime_error(std::string(what) + " is too large");
  const ossl::bio_ptr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
  if (!bio) throw std::runtime_error("Failed to allocate a buffer: " + openssl_error());
  ossl::x509_ptr cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
  if (!cert) throw std::runtime_error(std::string(what) + " is not a readable PEM certificate");
  return cert;
}

}  // namespace

pki_paths pki_paths::for_endpoint(const std::string &directory, const std::string &cn) {
  const boost::filesystem::path dir(directory);
  return pki_paths{(dir / (cn + ".crt")).string(), (dir / (cn + ".key")).string(), (dir / "ca.crt").string()};
}

void ensure_bootstrap_certificate(const pki_paths &paths, const std::string &cn) {
  if (boost::filesystem::exists(paths.certificate) && boost::filesystem::exists(paths.key)) return;

  const ossl::evp_pkey_ptr key(EVP_RSA_gen(4096));
  if (!key) throw std::runtime_error("Failed to generate a key: " + openssl_error());

  const ossl::x509_ptr cert(X509_new());
  if (!cert) throw std::runtime_error("Failed to allocate a certificate: " + openssl_error());
  X509_set_version(cert.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
  X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0);
  // Only used to ask for the real one; Icinga for Windows uses 3 months too.
  X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60 * 24 * 90);
  X509_set_pubkey(cert.get(), key.get());
  X509_NAME *name = X509_get_subject_name(cert.get());
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, reinterpret_cast<const unsigned char *>(cn.c_str()), -1, -1, 0);
  X509_set_issuer_name(cert.get(), name);
  if (!X509_sign(cert.get(), key.get(), EVP_sha256())) throw std::runtime_error("Failed to sign the bootstrap certificate: " + openssl_error());

  const ossl::bio_ptr key_bio = memory_bio();
  if (!PEM_write_bio_PrivateKey(key_bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr))
    throw std::runtime_error("Failed to encode the key: " + openssl_error());
  const ossl::bio_ptr cert_bio = memory_bio();
  if (!PEM_write_bio_X509(cert_bio.get(), cert.get())) throw std::runtime_error("Failed to encode the certificate: " + openssl_error());

  write_file(paths.key, bio_to_string(key_bio.get()), true);
  write_file(paths.certificate, bio_to_string(cert_bio.get()), false);
}

void store_signed_certificate(const pki_paths &paths, const std::string &cert_pem, const std::string &ca_pem) {
  // Validate both before replacing anything: a broken file would lock us out.
  parse_certificate(cert_pem, "The certificate in pki::UpdateCertificate");
  if (!ca_pem.empty()) parse_certificate(ca_pem, "The CA in pki::UpdateCertificate");
  if (!ca_pem.empty()) write_file(paths.ca, ca_pem, false);
  write_file(paths.certificate, cert_pem, false);
}

bool is_signed(const std::string &certificate) {
  const ossl::x509_ptr cert = load_certificate(certificate);
  if (!cert) return false;
  return X509_NAME_cmp(X509_get_subject_name(cert.get()), X509_get_issuer_name(cert.get())) != 0;
}

}  // namespace icinga_agent
