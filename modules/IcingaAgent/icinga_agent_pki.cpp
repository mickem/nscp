// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_pki.hpp"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <boost/filesystem.hpp>
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace icinga_agent {

namespace {

using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;
using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;

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
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Failed to write " + path);
    out << content;
  }
  if (secret) {
    boost::system::error_code ec;
    boost::filesystem::permissions(path, boost::filesystem::owner_read | boost::filesystem::owner_write, ec);
  }
}

X509_ptr load_certificate(const std::string &path) {
  X509_ptr none(nullptr, ::X509_free);
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) return none;
  X509 *cert = PEM_read_X509(f, nullptr, nullptr, nullptr);
  std::fclose(f);
  return X509_ptr(cert, ::X509_free);
}

}  // namespace

pki_paths pki_paths::for_endpoint(const std::string &directory, const std::string &cn) {
  const boost::filesystem::path dir(directory);
  return pki_paths{(dir / (cn + ".crt")).string(), (dir / (cn + ".key")).string(), (dir / "ca.crt").string()};
}

void ensure_bootstrap_certificate(const pki_paths &paths, const std::string &cn) {
  if (boost::filesystem::exists(paths.certificate) && boost::filesystem::exists(paths.key)) return;

  EVP_PKEY_ptr key(EVP_RSA_gen(4096), ::EVP_PKEY_free);
  if (!key) throw std::runtime_error("Failed to generate a key: " + openssl_error());

  X509_ptr cert(X509_new(), ::X509_free);
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

  BIO_ptr key_bio(BIO_new(BIO_s_mem()), ::BIO_free);
  PEM_write_bio_PrivateKey(key_bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr);
  BIO_ptr cert_bio(BIO_new(BIO_s_mem()), ::BIO_free);
  PEM_write_bio_X509(cert_bio.get(), cert.get());

  write_file(paths.key, bio_to_string(key_bio.get()), true);
  write_file(paths.certificate, bio_to_string(cert_bio.get()), false);
}

void store_signed_certificate(const pki_paths &paths, const std::string &cert_pem, const std::string &ca_pem) {
  // Validate before replacing anything: a broken file would lock us out.
  BIO_ptr bio(BIO_new_mem_buf(cert_pem.data(), static_cast<int>(cert_pem.size())), ::BIO_free);
  X509_ptr cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), ::X509_free);
  if (!cert) throw std::runtime_error("pki::UpdateCertificate carried an unreadable certificate");
  if (!ca_pem.empty()) write_file(paths.ca, ca_pem, false);
  write_file(paths.certificate, cert_pem, false);
}

bool is_signed(const std::string &certificate) {
  X509_ptr cert = load_certificate(certificate);
  if (!cert) return false;
  return X509_NAME_cmp(X509_get_subject_name(cert.get()), X509_get_issuer_name(cert.get())) != 0;
}

std::string certificate_cn(const std::string &certificate) {
  X509_ptr cert = load_certificate(certificate);
  if (!cert) return "";
  char buf[256] = {0};
  X509_NAME_get_text_by_NID(X509_get_subject_name(cert.get()), NID_commonName, buf, sizeof(buf));
  return buf;
}

}  // namespace icinga_agent
