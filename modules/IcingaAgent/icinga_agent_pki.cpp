// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_pki.hpp"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>

#include <boost/filesystem.hpp>
#include <limits>
#include <net/socket/socket_helpers.hpp>
#include <stdexcept>
#include <str/utf8.hpp>
#include <vector>

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

// Every certificate in a PEM stream: a CA file may hold more than one.
std::vector<ossl::x509_ptr> read_certificates(BIO *bio) {
  std::vector<ossl::x509_ptr> out;
  while (ossl::x509_ptr cert{PEM_read_bio_X509(bio, nullptr, nullptr, nullptr)}) out.push_back(std::move(cert));
  ERR_clear_error();  // the read past the last certificate
  return out;
}

std::vector<ossl::x509_ptr> parse_certificates(const std::string &pem, const char *what) {
  if (pem.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::runtime_error(std::string(what) + " is too large");
  const ossl::bio_ptr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
  if (!bio) throw std::runtime_error("Failed to allocate a buffer");
  std::vector<ossl::x509_ptr> certs = read_certificates(bio.get());
  if (certs.empty()) throw std::runtime_error(std::string(what) + " is not a readable PEM certificate");
  return certs;
}

std::vector<ossl::x509_ptr> load_certificates(const std::string &path) {
  const ossl::bio_ptr bio = open_file(path);
  if (!bio) return {};
  return read_certificates(bio.get());
}

// Whether `cert` was issued by one of `authorities`. Only the signature
// chain: validity dates are left to the TLS handshake, so an agent whose
// clock runs a little behind the master's does not refuse a certificate
// issued a moment ago.
void verify_issuer(X509 *cert, const std::vector<ossl::x509_ptr> &authorities) {
  const ossl::x509_store_ptr store(X509_STORE_new());
  const ossl::x509_store_ctx_ptr ctx(X509_STORE_CTX_new());
  if (!store || !ctx) throw std::runtime_error("Failed to allocate a certificate store");
  for (const ossl::x509_ptr &authority : authorities) X509_STORE_add_cert(store.get(), authority.get());
  ERR_clear_error();  // a duplicate in the bundle
  if (X509_STORE_CTX_init(ctx.get(), store.get(), cert, nullptr) != 1) throw std::runtime_error("Failed to set up certificate verification");
  X509_VERIFY_PARAM_set_flags(X509_STORE_CTX_get0_param(ctx.get()), X509_V_FLAG_NO_CHECK_TIME);
  if (X509_verify_cert(ctx.get()) != 1) {
    const std::string reason = X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx.get()));
    ERR_clear_error();
    throw std::runtime_error("The certificate in pki::UpdateCertificate was not issued by the Icinga CA: " + reason);
  }
}

bool matches_key(X509 *cert, EVP_PKEY *key) {
  const bool matches = X509_check_private_key(cert, key) == 1;
  if (!matches) ERR_clear_error();
  return matches;
}

void ensure_directory(const std::string &file) { boost::filesystem::create_directories(boost::filesystem::path(file).parent_path()); }

// A file written beside its target and renamed into place by commit(). Until
// then the target is untouched, and an abandoned one removes its temporary.
class staged_file {
 public:
  staged_file(const std::string &path, const std::string &content) : target_(path), temporary_(path + ".new") {
    boost::system::error_code ignored;
    boost::filesystem::remove(temporary_, ignored);
    try {
      socket_helpers::write_public_file(temporary_.string(), content);
    } catch (...) {
      boost::filesystem::remove(temporary_, ignored);
      throw;
    }
  }
  staged_file(const staged_file &) = delete;
  staged_file &operator=(const staged_file &) = delete;
  ~staged_file() {
    if (committed_) return;
    boost::system::error_code ignored;
    boost::filesystem::remove(temporary_, ignored);
  }

  void commit() {
    boost::system::error_code ec;
    boost::filesystem::rename(temporary_, target_, ec);
    if (ec) {
      // Windows fails a rename over a file that is read-only or held open;
      // take the target out of the way and retry, as write_private_file does.
      boost::system::error_code ignored;
      boost::filesystem::remove(target_, ignored);
      boost::filesystem::rename(temporary_, target_, ec);
    }
    if (ec) throw std::runtime_error("Failed to move " + temporary_.string() + " into place: " + utf8::utf8_from_native(ec.message()));
    committed_ = true;
  }

 private:
  boost::filesystem::path target_;
  boost::filesystem::path temporary_;
  bool committed_ = false;
};

}  // namespace

pki_paths pki_paths::for_endpoint(const std::string &directory, const std::string &cn) {
  const boost::filesystem::path dir(directory);
  return pki_paths{(dir / (cn + ".crt")).string(), (dir / (cn + ".key")).string(), (dir / "ca.crt").string()};
}

bool ensure_bootstrap_certificate(const pki_paths &paths, const std::string &cn) {
  const bool present = boost::filesystem::exists(paths.certificate) && boost::filesystem::exists(paths.key);
  if (present) {
    const ossl::x509_ptr cert = load_certificate(paths.certificate);
    const ossl::evp_pkey_ptr key = load_private_key(paths.key);
    ERR_clear_error();
    if (cert && key && matches_key(cert.get(), key.get())) return false;
  }
  // The key size Icinga itself uses for agents. The certificate is only used
  // to ask for the real one, so a short validity is enough; Icinga for Windows
  // uses three months too.
  const socket_helpers::generated_certificate generated = socket_helpers::generate_self_signed(cn, 4096, 90, false);
  ensure_directory(paths.key);
  // Stopped between the two, the new key sits beside a missing or an older
  // certificate: not a pair, so the next start generates both again.
  staged_file certificate(paths.certificate, generated.cert_pem);
  socket_helpers::write_private_file(paths.key, generated.key_pem);
  certificate.commit();
  return present;
}

void store_signed_certificate(const pki_paths &paths, const std::string &cn, const std::string &cert_pem, const std::string &ca_pem) {
  // Validate everything before replacing anything: a certificate we cannot
  // use would lock the agent out until someone deleted it by hand.
  const std::vector<ossl::x509_ptr> certs = parse_certificates(cert_pem, "The certificate in pki::UpdateCertificate");
  X509 *cert = certs.front().get();
  const ossl::evp_pkey_ptr key = load_private_key(paths.key);
  if (!key) throw std::runtime_error("Cannot read the agent's own key at " + paths.key);
  if (!matches_key(cert, key.get())) throw std::runtime_error("The certificate in pki::UpdateCertificate is not for this agent's key");
  const std::string subject = socket_helpers::format_subject_cn_only(cert);
  if (subject != cn) {
    throw std::runtime_error("The certificate in pki::UpdateCertificate is for '" + socket_helpers::escape_for_log(subject) + "', not for this agent, '" + cn +
                             "'");
  }
  // Issued by the CA that came with it - the one the agent trusts from here
  // on - or, when the parent sent none, by the one already stored.
  const std::vector<ossl::x509_ptr> authorities =
      ca_pem.empty() ? load_certificates(paths.ca) : parse_certificates(ca_pem, "The CA in pki::UpdateCertificate");
  if (authorities.empty()) throw std::runtime_error("pki::UpdateCertificate carried no CA and none is stored at " + paths.ca);
  verify_issuer(cert, authorities);

  ensure_directory(paths.certificate);
  // Both written in full before either replaces anything. The CA goes first:
  // stopped between the two, the agent verifies the parent against the new
  // CA with its old certificate and simply asks again.
  staged_file certificate(paths.certificate, cert_pem);
  if (!ca_pem.empty()) {
    staged_file ca(paths.ca, ca_pem);
    ca.commit();
  }
  certificate.commit();
}

bool is_signed(const std::string &certificate) {
  const ossl::x509_ptr cert = load_certificate(certificate);
  if (!cert) return false;
  return X509_NAME_cmp(X509_get_subject_name(cert.get()), X509_get_issuer_name(cert.get())) != 0;
}

}  // namespace icinga_agent
