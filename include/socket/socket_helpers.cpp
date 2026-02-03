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

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <socket/socket_helpers.hpp>
#include <str/format.hpp>
#include <str/utils.hpp>
#include <utf8.hpp>
#ifndef WIN32
#define OPENSSL_NO_CRYPTO_MDEBUG
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>
#endif
const int socket_helpers::connection_info::backlog_default = 0;

namespace ip = boost::asio::ip;

std::list<std::string> socket_helpers::connection_info::validate() const { return validate_ssl(); }
void socket_helpers::validate_certificate(const std::string &certificate, std::list<std::string> &list) {
#ifdef USE_SSL
  if (!certificate.empty() && !boost::filesystem::is_regular_file(certificate)) {
    const auto parent_path = boost::filesystem::path(certificate).parent_path();
    if (!exists(parent_path)) {
      boost::filesystem::create_directories(parent_path);
      list.emplace_back("Creating certificate folder: " + parent_path.string());
    }
    if (boost::algorithm::ends_with(certificate, "/certificate.pem")) {
      list.emplace_back("Certificate not found: " + certificate + " (generating a default certificate)");
      try {
        write_certs(certificate, false);
      } catch (const std::exception &e) {
        list.emplace_back(e.what());
      }
    } else if (boost::algorithm::ends_with(certificate, "/ca.pem")) {
      list.emplace_back("CA not found: " + certificate + " (generating a default CA)");
      try {
        write_certs(certificate, true);
      } catch (const std::exception &e) {
        list.emplace_back(e.what());
      }
    } else
      list.emplace_back("Certificate not found: " + certificate);
  }
#else
  list.emplace_back("SSL is not supported (not compiled with openssl)");
#endif
}

std::list<std::string> socket_helpers::connection_info::validate_ssl() const {
  std::list<std::string> list;
  if (!ssl.enabled) return list;
#ifndef USE_SSL
  list.push_back("SSL is not supported (not compiled with openssl)");
#endif

#ifdef USE_SSL
  validate_certificate(ssl.certificate, list);
  validate_certificate(ssl.ca_path, list);
  if (!ssl.certificate_key.empty() && !boost::filesystem::is_regular_file(ssl.certificate_key))
    list.push_back("Certificate key not found: " + ssl.certificate_key);
  if (!ssl.dh_key.empty() && !boost::filesystem::is_regular_file(ssl.dh_key)) list.push_back("DH key not found: " + ssl.dh_key);
#endif
  return list;
}

long socket_helpers::connection_info::get_ctx_opts() const {
  long opts = 0;
#ifdef USE_SSL
  opts |= ssl.get_ctx_opts();
#endif
  return opts;
}

void socket_helpers::io::set_result(boost::optional<boost::system::error_code> *a, const boost::system::error_code &b) {
  if (!b) {
    a->reset(b);
  }
}
#ifdef USE_SSL
void socket_helpers::connection_info::ssl_opts::configure_ssl_context(boost::asio::ssl::context &context, std::list<std::string> &errors) const {
  boost::system::error_code er;
  if (!certificate.empty() && certificate != "none") {
    context.use_certificate_chain_file(certificate, er);
    if (er) errors.push_back("Failed to load certificate " + certificate + ": " + utf8::utf8_from_native(er.message()));
    if (!certificate_key.empty() && certificate_key != "none") {
      context.use_private_key_file(certificate_key, get_certificate_key_format(), er);
      if (er) errors.push_back("Failed to load certificate key " + certificate_key + ": " + utf8::utf8_from_native(er.message()));
    } else {
      context.use_private_key_file(certificate, get_certificate_key_format(), er);
      if (er) errors.push_back("Failed to load certificate (as key) " + certificate + ": " + utf8::utf8_from_native(er.message()));
    }
  }
  context.set_verify_mode(get_verify_mode(), er);
  if (er) errors.push_back("Failed to set verify mode: " + utf8::utf8_from_native(er.message()));
  if (SSL_CTX_set_min_proto_version(context.native_handle(), get_tls_min_version()) == 0) {
    errors.emplace_back("Failed to set min tls version");
  }
  if (SSL_CTX_set_max_proto_version(context.native_handle(), get_tls_max_version()) == 0) {
    errors.emplace_back("Failed to set max tls version");
  }
  if (!allowed_ciphers.empty()) {
    ::ERR_clear_error();
    if (SSL_CTX_set_cipher_list(context.native_handle(), allowed_ciphers.c_str()) == 0) {
      errors.push_back("Failed to set ciphers " + allowed_ciphers + ": " + utf8::utf8_from_native(ERR_reason_error_string(ERR_get_error())));
    }
  }
  if (!dh_key.empty() && dh_key != "none") {
    context.use_tmp_dh_file(dh_key, er);
    if (er) errors.push_back("Failed to set dh file " + dh_key + ": " + utf8::utf8_from_native(er.message()));
  }

  if (!ca_path.empty()) {
    context.load_verify_file(ca_path, er);
    if (er) errors.push_back("Failed to load CA " + ca_path + ": " + utf8::utf8_from_native(er.message()));
  }
  if (debug_verify) {
    context.set_verify_callback([](bool preverified, boost::asio::ssl::verify_context &v_ctx) -> bool {
      char subject_name[256];
      X509 *cert = X509_STORE_CTX_get_current_cert(v_ctx.native_handle());
      X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
      int error_code = X509_STORE_CTX_get_error(v_ctx.native_handle());
      std::cout << "Verifying: " << subject_name << std::endl;
      std::cout << "  Preverified: " << (preverified ? "Yes" : "No") << std::endl;
      std::cout << "  Error: " << X509_verify_cert_error_string(error_code) << std::endl;
      return preverified;
    });
  }
}

boost::asio::ssl::context::verify_mode socket_helpers::connection_info::ssl_opts::get_verify_mode() const {
  boost::asio::ssl::context::verify_mode mode = boost::asio::ssl::context_base::verify_none;
  for (const std::string &key : str::utils::split_lst(verify_mode, std::string(","))) {
    if (key == "client-once")
      mode |= boost::asio::ssl::context_base::verify_client_once;
    else if (key == "none")
      mode |= boost::asio::ssl::context_base::verify_none;
    else if (key == "peer")
      mode |= boost::asio::ssl::context_base::verify_peer;
    else if (key == "fail-if-no-cert")
      mode |= boost::asio::ssl::context_base::verify_fail_if_no_peer_cert;
    else if (key == "peer-cert") {
      mode |= boost::asio::ssl::context_base::verify_peer;
      mode |= boost::asio::ssl::context_base::verify_fail_if_no_peer_cert;
    } else if (key == "workarounds")
      mode |= boost::asio::ssl::context_base::default_workarounds;
    else if (key == "single")
      mode |= boost::asio::ssl::context::single_dh_use;
  }
  return mode;
}

long socket_helpers::connection_info::ssl_opts::get_tls_min_version() const {
  std::string tmp = boost::algorithm::to_lower_copy(tls_version);
  str::utils::replace(tmp, "+", "");
  if (tmp == "tlsv1.3" || tmp == "tls1.3" || tmp == "1.3") {
    return TLS1_3_VERSION;
  }
  if (tmp == "tlsv1.2" || tmp == "tls1.2" || tmp == "1.2") {
    return TLS1_2_VERSION;
  }
  if (tmp == "tlsv1.1" || tmp == "tls1.1" || tmp == "1.1") {
    return TLS1_1_VERSION;
  }
  if (tmp == "tlsv1.0" || tmp == "tls1.0" || tmp == "1.0") {
    return TLS1_VERSION;
  }
  if (tmp == "sslv3" || tmp == "ssl3") {
    return SSL3_VERSION;
  }
  throw socket_exception("Invalid tls version: " + tmp);
}

long socket_helpers::connection_info::ssl_opts::get_tls_max_version() const {
  std::string tmp = boost::algorithm::to_lower_copy(tls_version);
  if (tmp == "tlsv1.3" || tmp == "tls1.3" || tmp == "1.3" || tmp == "tlsv1.2+" || tmp == "tls1.2+" || tmp == "1.2+" || tmp == "tlsv1.1+" || tmp == "tls1.1+" ||
      tmp == "1.1+" || tmp == "sslv3+" || tmp == "ssl3+") {
    return TLS1_3_VERSION;
  }
  if (tmp == "tlsv1.2" || tmp == "tls1.2" || tmp == "1.2") {
    return TLS1_2_VERSION;
  }
  if (tmp == "tlsv1.1" || tmp == "tls1.1" || tmp == "1.1") {
    return TLS1_1_VERSION;
  }
  if (tmp == "tlsv1.0" || tmp == "tls1.0" || tmp == "1.0") {
    return TLS1_VERSION;
  }
  if (tmp == "sslv3" || tmp == "ssl3") {
    return SSL3_VERSION;
  }
  throw socket_exception("Invalid tls version: " + tmp);
}

boost::asio::ssl::context::file_format socket_helpers::connection_info::ssl_opts::get_certificate_format() const {
  if (certificate_format == "asn1") return boost::asio::ssl::context::asn1;
  return boost::asio::ssl::context::pem;
}

boost::asio::ssl::context::file_format socket_helpers::connection_info::ssl_opts::get_certificate_key_format() const {
  if (certificate_key_format == "asn1") return boost::asio::ssl::context::asn1;
  return boost::asio::ssl::context::pem;
}
long socket_helpers::connection_info::ssl_opts::get_ctx_opts() const {
  long opts = 0;
  for (const std::string &key : str::utils::split_lst(ssl_options, std::string(","))) {
    if (key == "default-workarounds") opts |= boost::asio::ssl::context::default_workarounds;
    if (key == "no-sslv2") opts |= boost::asio::ssl::context::no_sslv2;
    if (key == "no-sslv3") opts |= boost::asio::ssl::context::no_sslv3;
    if (key == "no-tlsv1") opts |= boost::asio::ssl::context::no_tlsv1;
    if (key == "no-tlsv1_1") opts |= boost::asio::ssl::context::no_tlsv1_1;
    if (key == "no-tlsv1_2") opts |= boost::asio::ssl::context::no_tlsv1_2;
    if (key == "no-tlsv1_3") opts |= boost::asio::ssl::context::no_tlsv1_3;
    if (key == "single-dh-use") opts |= boost::asio::ssl::context::single_dh_use;
  }
  return opts;
}

void genkey_callback(int, int, void *) {
  // Ignored as we dont want to show progress...
}

int add_ext(X509 *cert, const int nid, const char *value) {
  const std::size_t len = strlen(value);
  const auto tmp = new char[len + 10];
  strncpy(tmp, value, len);
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
  X509_EXTENSION *ex = X509V3_EXT_conf_nid(nullptr, &ctx, nid, tmp);
  delete[] tmp;
  if (!ex) return 0;
  X509_add_ext(cert, ex, -1);
  X509_EXTENSION_free(ex);
  return 1;
}

using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;
using BIGNUM_ptr = std::unique_ptr<BIGNUM, decltype(&::BN_free)>;
using X509_EXTENSION_ptr = std::unique_ptr<X509_EXTENSION, decltype(&::X509_EXTENSION_free)>;
using EVP_PKEY_CTX_ptr = std::unique_ptr<EVP_PKEY_CTX, decltype(&::EVP_PKEY_CTX_free)>;
using ASN1_INTEGER_ptr = std::unique_ptr<ASN1_INTEGER, decltype(&::ASN1_INTEGER_free)>;

std::string get_open_ssl_error() {
  std::stringstream ss;
  unsigned long err_code = ERR_get_error();
  while (err_code) {
    char err_buf[256];
    ERR_error_string_n(err_code, err_buf, sizeof(err_buf));
    ss << err_buf << std::endl;
    err_code = ERR_get_error();
  }
  return ss.str();
}

void make_certificate(const X509_ptr &cert, EVP_PKEY_ptr &pkey, const int bits, const int days, bool ca) {
  EVP_PKEY_CTX_ptr pctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
  if (!pctx) {
    throw socket_helpers::socket_exception("Failed to create EVP_PKEY_CTX: " + get_open_ssl_error());
  }
  if (EVP_PKEY_keygen_init(pctx.get()) <= 0) {
    throw socket_helpers::socket_exception("Failed to initialize keygen: " + get_open_ssl_error());
  }
  if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx.get(), bits) <= 0) {
    throw socket_helpers::socket_exception("Failed to set RSA keygen bits: " + get_open_ssl_error());
  }

  EVP_PKEY *pkey_raw = pkey.release();  // release ownership to keygen
  const auto ret = EVP_PKEY_keygen(pctx.get(), &pkey_raw);
  pkey.reset(pkey_raw);  // take ownership back
  if (ret <= 0) {
    throw socket_helpers::socket_exception("Failed to generate key: " + get_open_ssl_error());
  }

  if (X509_set_version(cert.get(), 2) == 0) {
    throw socket_helpers::socket_exception("Failed to set X509 version: " + get_open_ssl_error());
  }

  const BIGNUM_ptr bn(BN_new(), BN_free);
  if (!bn) {
    throw socket_helpers::socket_exception("Failed to create BIGNUM: " + get_open_ssl_error());
  }
  if (BN_rand(bn.get(), 128, -1, 0) == 0) {
    throw socket_helpers::socket_exception("Failed to generate random BIGNUM: " + get_open_ssl_error());
  }
  const ASN1_INTEGER_ptr serial_instance(ASN1_INTEGER_new(), ASN1_INTEGER_free);
  if (!serial_instance) {
    throw socket_helpers::socket_exception("Failed to create ASN1_INTEGER: " + get_open_ssl_error());
  }
  BN_to_ASN1_INTEGER(bn.get(), serial_instance.get());
  if (X509_set_serialNumber(cert.get(), serial_instance.get()) == 0) {
    throw socket_helpers::socket_exception("Failed to set serial number: " + get_open_ssl_error());
  }

  X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0);
  X509_gmtime_adj(X509_getm_notAfter(cert.get()), days * 24 * 3600);

  if (X509_set_pubkey(cert.get(), pkey.get()) == 0) {
    throw socket_helpers::socket_exception("Failed to set public key: " + get_open_ssl_error());
  }

  X509_NAME *name = X509_get_subject_name(cert.get());
  if (!name) {
    throw socket_helpers::socket_exception("Failed to get subject name: " + get_open_ssl_error());
  }

  if (X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("SE"), -1, -1, 0) == 0) {
    throw socket_helpers::socket_exception("Failed to add C: " + get_open_ssl_error());
  }
  if (X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("NSClient++"), -1, -1, 0) == 0) {
    throw socket_helpers::socket_exception("Failed to add O: " + get_open_ssl_error());
  }
  if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("generated-certificate"), -1, -1, 0) == 0) {
    throw socket_helpers::socket_exception("Failed to add CN: " + get_open_ssl_error());
  }

  if (X509_set_issuer_name(cert.get(), name) == 0) {
    throw socket_helpers::socket_exception("Failed to set issuer name: " + get_open_ssl_error());
  }

  add_ext(cert.get(), NID_subject_key_identifier, "hash");
  add_ext(cert.get(), NID_authority_key_identifier, "keyid:always,issuer");
  add_ext(cert.get(), NID_subject_alt_name, "DNS:localhost,IP:127.0.0.1");

  if (ca) {
    add_ext(cert.get(), NID_basic_constraints, "critical,CA:TRUE");
    add_ext(cert.get(), NID_key_usage, "critical,keyCertSign,cRLSign");
    add_ext(cert.get(), NID_netscape_cert_type, "sslCA");
  }

  if (X509_sign(cert.get(), pkey.get(), EVP_sha256()) == 0) {
    throw socket_helpers::socket_exception("Failed to sign certificate: " + get_open_ssl_error());
  }
}

void socket_helpers::write_certs(const std::string &cert, const bool ca) {
  const X509_ptr certificate_instance(X509_new(), X509_free);
  if (!certificate_instance) {
    throw socket_exception("Failed to create X509 object: " + get_open_ssl_error());
  }
  EVP_PKEY_ptr private_key_instance(EVP_PKEY_new(), EVP_PKEY_free);
  if (!private_key_instance) {
    throw socket_exception("Failed to create private key: " + get_open_ssl_error());
  }

  make_certificate(certificate_instance, private_key_instance, 2048, 365, ca);

  const BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  if (!PEM_write_bio_PKCS8PrivateKey(bio.get(), private_key_instance.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
    throw socket_exception("Failed to serialize key to " + cert);
  }
  if (!PEM_write_bio_X509(bio.get(), certificate_instance.get())) {
    throw socket_exception("Failed to serialize certificate to " + cert);
  }

  const std::size_t size = BIO_ctrl_pending(bio.get());
  const auto buf = std::make_unique<char[]>(size);
  if (BIO_read(bio.get(), buf.get(), static_cast<int>(size)) < 0) {
    throw socket_exception("Failed to read serialized key");
  }

  FILE *file = fopen(cert.c_str(), "wb");
  if (file == nullptr) throw socket_exception("Failed to write certificate to: " + cert);
  fwrite(buf.get(), sizeof(char), size, file);
  fclose(file);
}

boost::asio::ssl::context_base::method socket_helpers::tls_method_parser(const std::string &tls_version) {
  std::string tmp = boost::algorithm::to_lower_copy(tls_version);
  str::utils::replace(tmp, "+", "");
  if (tmp == "tlsv1.3" || tmp == "tls1.3" || tmp == "1.3") {
    return boost::asio::ssl::context::tlsv13;
  }
  if (tmp == "tlsv1.2" || tmp == "tls1.2" || tmp == "1.2") {
    return boost::asio::ssl::context::tlsv12;
  }
  if (tmp == "tlsv1.1" || tmp == "tls1.1" || tmp == "1.1") {
    return boost::asio::ssl::context::tlsv11;
  }
  if (tmp == "tlsv1.0" || tmp == "tls1.0" || tmp == "1.0") {
    return boost::asio::ssl::context::tlsv1;
  }
  if (tmp == "sslv3" || tmp == "ssl3") {
    return boost::asio::ssl::context::sslv23;
  }
  throw socket_exception("Invalid tls version: " + tmp);
}

boost::asio::ssl::verify_mode socket_helpers::verify_mode_parser(const std::string &verify_mode) {
  boost::asio::ssl::verify_mode mode = boost::asio::ssl::verify_none;
  for (const std::string &key : str::utils::split_lst(verify_mode, std::string(","))) {
    if (key == "none")
      mode |= boost::asio::ssl::verify_none;
    else if (key == "peer" || key == "certificate")
      mode |= boost::asio::ssl::verify_peer;
    else if (key == "fail-if-no-cert" || key == "fail-if-no-peer-cert" || key == "client-certificate")
      mode |= boost::asio::ssl::verify_fail_if_no_peer_cert;
    else if (key == "peer-cert") {
      mode |= boost::asio::ssl::verify_peer;
      mode |= boost::asio::ssl::verify_fail_if_no_peer_cert;
    } else
      throw socket_exception("Invalid tls verify mode: " + key);
  }
  return mode;
}

#endif
