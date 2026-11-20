// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_pki.hpp"

#include <openssl/err.h>
#include <openssl/pem.h>

#include <boost/filesystem.hpp>
#include <limits>
#include <net/socket/socket_helpers.hpp>
#include <stdexcept>

#include "icinga_agent_openssl.hpp"

namespace icinga_agent {

namespace {

// Read through a BIO rather than a FILE*: OpenSSL's FILE* entry points need
// the applink shim on Windows when the CRTs differ, a BIO opened by OpenSSL
// does not.
ossl::bio_ptr open_file(const std::string &path) {
  ossl::bio_ptr bio(BIO_new_file(path.c_str(), "rb"));
  if (!bio) ERR_clear_error();
  return bio;
}

ossl::x509_ptr load_certificate(const std::string &path) {
  const ossl::bio_ptr bio = open_file(path);
  if (!bio) return {};
  return ossl::x509_ptr(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
}

ossl::evp_pkey_ptr load_private_key(const std::string &path) {
  const ossl::bio_ptr bio = open_file(path);
  if (!bio) return {};
  return ossl::evp_pkey_ptr(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
}

ossl::x509_ptr parse_certificate(const std::string &pem, const char *what) {
  if (pem.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::runtime_error(std::string(what) + " is too large");
  const ossl::bio_ptr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
  if (!bio) throw std::runtime_error("Failed to allocate a buffer");
  ossl::x509_ptr cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
  if (!cert) throw std::runtime_error(std::string(what) + " is not a readable PEM certificate");
  return cert;
}

void ensure_directory(const std::string &file) { boost::filesystem::create_directories(boost::filesystem::path(file).parent_path()); }

}  // namespace

pki_paths pki_paths::for_endpoint(const std::string &directory, const std::string &cn) {
  const boost::filesystem::path dir(directory);
  return pki_paths{(dir / (cn + ".crt")).string(), (dir / (cn + ".key")).string(), (dir / "ca.crt").string()};
}

void ensure_bootstrap_certificate(const pki_paths &paths, const std::string &cn) {
  if (boost::filesystem::exists(paths.certificate) && boost::filesystem::exists(paths.key)) return;
  // The key size Icinga itself uses for agents. The certificate is only used
  // to ask for the real one, so a short validity is enough; Icinga for Windows
  // uses three months too.
  const socket_helpers::generated_certificate generated = socket_helpers::generate_self_signed(cn, 4096, 90, false);
  ensure_directory(paths.key);
  socket_helpers::write_private_file(paths.key, generated.key_pem);
  socket_helpers::write_public_file(paths.certificate, generated.cert_pem);
}

void store_signed_certificate(const pki_paths &paths, const std::string &cert_pem, const std::string &ca_pem) {
  // Validate everything before replacing anything: a certificate we cannot
  // use would lock the agent out until someone deleted it by hand.
  const ossl::x509_ptr cert = parse_certificate(cert_pem, "The certificate in pki::UpdateCertificate");
  if (!ca_pem.empty()) parse_certificate(ca_pem, "The CA in pki::UpdateCertificate");
  const ossl::evp_pkey_ptr key = load_private_key(paths.key);
  if (!key) throw std::runtime_error("Cannot read the agent's own key at " + paths.key);
  if (X509_check_private_key(cert.get(), key.get()) != 1) {
    ERR_clear_error();
    throw std::runtime_error("The certificate in pki::UpdateCertificate is not for this agent's key");
  }
  ensure_directory(paths.certificate);
  if (!ca_pem.empty()) socket_helpers::write_public_file(paths.ca, ca_pem);
  socket_helpers::write_public_file(paths.certificate, cert_pem);
}

bool is_signed(const std::string &certificate) {
  const ossl::x509_ptr cert = load_certificate(certificate);
  if (!cert) return false;
  return X509_NAME_cmp(X509_get_subject_name(cert.get()), X509_get_issuer_name(cert.get())) != 0;
}

}  // namespace icinga_agent
