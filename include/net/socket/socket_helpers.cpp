// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <iomanip>
#include <net/socket/socket_helpers.hpp>
#include <sstream>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <vector>
#if defined(USE_SSL) && !defined(WIN32)
#define OPENSSL_NO_CRYPTO_MDEBUG
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>
#endif
#ifdef USE_SSL
// Pulled in here (not gated on !WIN32) because extract_peer_subject_dn
// needs the X509/BIO/SSL_get_peer_certificate symbols on every platform
// that builds with SSL support. boost::asio::ssl already drags openssl
// in on Windows, so this is just making the symbol visibility explicit.
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif
#ifdef WIN32
// After boost/asio.hpp, which pulls winsock2.h in first; accctrl/aclapi then
// depend on windows.h's types.
#include <windows.h>
//
#include <accctrl.h>
#include <aclapi.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
const int socket_helpers::connection_info::backlog_default = 0;

namespace ip = boost::asio::ip;

std::list<std::string> socket_helpers::connection_info::validate() const { return validate_ssl(); }

namespace {
std::string keep_verbatim(std::string value) { return value; }

// Find the address this machine is best known by in the given family, for the
// ${address_ipv4} / ${address_ipv6*} placeholders (#349). Loopback, link-local
// and unspecified addresses never identify a host to a remote monitoring
// server, so they are rejected everywhere; none() means "this machine has no
// usable address in this family" and the caller leaves the token unresolved.
boost::optional<ip::address> discover_local_address(const bool ipv6) {
  // Routing-table probe: connecting a UDP socket sends no packet, it only asks
  // the kernel which source address it would use for that destination - which
  // is exactly the address a remote server would see this machine as. The
  // probe destinations are from the documentation ranges (TEST-NET-3 and
  // 2001:db8::/32) so nothing real is ever addressed.
  try {
    boost::asio::io_context io;
    ip::udp::socket probe(io);
    const ip::udp::endpoint target =
        ipv6 ? ip::udp::endpoint(ip::make_address_v6("2001:db8::1"), 53) : ip::udp::endpoint(ip::make_address_v4("203.0.113.1"), 53);
    probe.connect(target);
    const ip::address addr = probe.local_endpoint().address();
    if (!addr.is_loopback() && !addr.is_unspecified() && !(addr.is_v6() && addr.to_v6().is_link_local())) return addr;
  } catch (const std::exception &) {
    // No route in this family (common for IPv6) - try the resolver below.
  }
  // Fall back to what the host name resolves to. This can disagree with the
  // routing answer (a 127.0.1.1 hosts-file entry, a multi-homed machine), so
  // it is only consulted when the routing probe has nothing to offer.
  try {
    boost::asio::io_context io;
    ip::udp::resolver resolver(io);
    for (const auto &entry : resolver.resolve(ipv6 ? ip::udp::v6() : ip::udp::v4(), ip::host_name(), "")) {
      const ip::address addr = entry.endpoint().address();
      if (addr.is_loopback() || addr.is_unspecified()) continue;
      if (addr.is_v6() && addr.to_v6().is_link_local()) continue;
      return addr;
    }
  } catch (const std::exception &) {
  }
  return boost::none;
}

// One host name placeholder substitution pass; `prep` is applied to every
// value before it is spliced in (identity for a host name spec, the path
// sanitizer for a string that ends up on the file system).
std::string expand_placeholders_with(std::string spec, std::string (*prep)(std::string)) {
  // Nothing to resolve, and asking the OS for its host name or addresses is
  // not free (and can even throw): a settings context or an attachment path
  // usually has no placeholder at all. Every token starts with ${host,
  // ${domain or ${address_ip, so this also skips a spec that only carries path
  // tokens (${shared-path}).
  const bool has_name_token = spec.find("${host") != std::string::npos || spec.find("${domain") != std::string::npos;
  const bool has_address_token = spec.find("${address_ip") != std::string::npos;
  if (!has_name_token && !has_address_token) return spec;

  if (has_name_token) {
    std::string host_name;
    try {
      host_name = ip::host_name();
    } catch (const std::exception &) {
      // No host name to substitute with. Leave the placeholders alone rather
      // than let a failing gethostname() abort settings boot - the callers all
      // have a defined answer for an unresolved token.
      host_name.clear();
    }
    if (!host_name.empty()) {
      // The full name exactly as the system reports it. ${host} stops at the
      // first '.', so without this there is no way to get the fqdn from inside
      // a template - only by setting the whole spec to "auto", which a
      // template cannot do.
      str::utils::replace(spec, "${hostname_uc}", prep(boost::algorithm::to_upper_copy(host_name)));
      str::utils::replace(spec, "${hostname_lc}", prep(boost::algorithm::to_lower_copy(host_name)));
      str::utils::replace(spec, "${hostname}", prep(host_name));

      const str::utils::token dn = str::utils::getToken(host_name, '.');
      str::utils::replace(spec, "${host}", prep(dn.first));
      str::utils::replace(spec, "${domain}", prep(dn.second));
      str::utils::replace(spec, "${host_uc}", prep(boost::algorithm::to_upper_copy(dn.first)));
      str::utils::replace(spec, "${domain_uc}", prep(boost::algorithm::to_upper_copy(dn.second)));
      str::utils::replace(spec, "${host_lc}", prep(boost::algorithm::to_lower_copy(dn.first)));
      str::utils::replace(spec, "${domain_lc}", prep(boost::algorithm::to_lower_copy(dn.second)));
    }
  }

  if (has_address_token) {
    // A machine with no usable address in a family keeps that family's tokens
    // unresolved, same as the host name tokens above when gethostname() fails.
    if (spec.find("${address_ipv4}") != std::string::npos) {
      if (const boost::optional<ip::address> v4 = discover_local_address(false)) str::utils::replace(spec, "${address_ipv4}", prep(v4->to_string()));
    }
    if (spec.find("${address_ipv6") != std::string::npos) {
      if (const boost::optional<ip::address> v6 = discover_local_address(true)) {
        const ip::address_v6 addr = v6->to_v6();
        str::utils::replace(spec, "${address_ipv6_lc_comp}", prep(socket_helpers::format_ipv6(addr, false, true)));
        str::utils::replace(spec, "${address_ipv6_lc_uncomp}", prep(socket_helpers::format_ipv6(addr, false, false)));
        str::utils::replace(spec, "${address_ipv6_uc_comp}", prep(socket_helpers::format_ipv6(addr, true, true)));
        str::utils::replace(spec, "${address_ipv6_uc_uncomp}", prep(socket_helpers::format_ipv6(addr, true, false)));
        str::utils::replace(spec, "${address_ipv6_lc}", prep(socket_helpers::format_ipv6(addr, false, true)));
        str::utils::replace(spec, "${address_ipv6_uc}", prep(socket_helpers::format_ipv6(addr, true, true)));
        str::utils::replace(spec, "${address_ipv6}", prep(socket_helpers::format_ipv6(addr, false, true)));
      }
    }
  }
  return spec;
}
}  // namespace

std::string socket_helpers::format_ipv6(const boost::asio::ip::address_v6 &address, const bool uppercase, const bool compressed) {
  std::string formatted;
  if (compressed) {
    // to_string() is the RFC 5952 canonical form: lowercase hex digits with
    // the longest zero run elided.
    formatted = address.to_string();
  } else {
    // All eight groups, zero-padded to four digits: the fixed-width form some
    // inventory/monitoring systems key their host records on.
    const boost::asio::ip::address_v6::bytes_type bytes = address.to_bytes();
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); i += 2) {
      if (i > 0) oss << ':';
      oss << std::setw(2) << static_cast<unsigned int>(bytes[i]) << std::setw(2) << static_cast<unsigned int>(bytes[i + 1]);
    }
    formatted = oss.str();
  }
  if (uppercase) boost::algorithm::to_upper(formatted);
  return formatted;
}

std::string socket_helpers::sanitize_path_component(std::string value) {
  // gethostname() is not fully under the operator's control - DHCP can set it
  // on some systems - so a value substituted into a local path must not be
  // able to smuggle in a separator or a ".." component. A real host name is
  // RFC-952 material (letters, digits, '-', '.' between labels), so mapping
  // everything else to '_' cannot change a legitimate setup.
  bool only_dots = !value.empty();
  for (char &c : value) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    if (!ok) c = '_';
    if (c != '.') only_dots = false;
  }
  // "." or ".." as a whole component is a path, not a name.
  if (only_dots) return "_";
  return value;
}

std::string socket_helpers::expand_hostname_placeholders(std::string spec) { return expand_placeholders_with(std::move(spec), keep_verbatim); }

std::string socket_helpers::expand_hostname_placeholders_in_path(std::string spec) {
  return expand_placeholders_with(std::move(spec), &socket_helpers::sanitize_path_component);
}

std::string socket_helpers::expand_hostname(std::string spec) {
  // The "auto" shorthands reinterpret the whole spec, so they only apply where
  // the string *is* a host name - not to the strings which merely contain a
  // placeholder (a settings context, an attachment target).
  if (spec == "auto") return ip::host_name();
  if (spec == "auto-lc") return boost::algorithm::to_lower_copy(ip::host_name());
  if (spec == "auto-uc") return boost::algorithm::to_upper_copy(ip::host_name());
  return expand_hostname_placeholders(std::move(spec));
}
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
        list.emplace_back("CA private key written to: " + ca_key_path(certificate) + " (keep it, do not distribute it)");
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

#ifdef USE_SSL
std::string socket_helpers::format_subject_dn_rfc2253(void *x509) {
  if (!x509) return {};
  const auto cert = static_cast<X509 *>(x509);
  std::string result;
  // X509_get_subject_name returns an internal pointer owned by the
  // cert - do NOT free it. Const-cast because the OpenSSL signature
  // for X509_NAME_print_ex takes a non-const pointer (the function
  // itself is read-only; this is purely a header signature concern).
  const X509_NAME *subject = X509_get_subject_name(cert);
  if (!subject) return result;
  BIO *bio = BIO_new(BIO_s_mem());
  if (!bio) return result;
  // RFC 2253 (XN_FLAG_RFC2253) is the canonical, sortable, escape-safe
  // representation; structural specials (comma, plus, equals, leading
  // hash, leading/trailing space) are backslash-escaped. We clear
  // ASN1_STRFLGS_ESC_MSB so high-bit bytes pass through as raw UTF-8
  // instead of being hex-escaped (`\C3\A9`). RFC 2253 explicitly
  // allows UTF-8 in DN values; the hex-escape form would surprise
  // operators who paste DN strings from cert tooling, and any future
  // identity-map indirection comparing DNs as strings would mismatch
  // an issued-as-UTF-8 cert against an operator-typed handle.
  const unsigned long fmt = XN_FLAG_RFC2253 & ~ASN1_STRFLGS_ESC_MSB;
  if (X509_NAME_print_ex(bio, subject, 0, fmt) > 0) {
    BUF_MEM *mem = nullptr;
    BIO_get_mem_ptr(bio, &mem);
    if (mem && mem->data && mem->length > 0) {
      result.assign(mem->data, mem->length);
    }
  }
  BIO_free(bio);
  return result;
}

std::string socket_helpers::extract_peer_subject_dn(void *ssl) {
  if (!ssl) return {};
  const SSL *ssl_instance = static_cast<SSL *>(ssl);
  // SSL_get_peer_certificate bumps the refcount; we own this pointer and
  // must free it on every return path.
  X509 *cert = SSL_get_peer_certificate(ssl_instance);
  if (!cert) return {};
  const std::string result = format_subject_dn_rfc2253(cert);
  X509_free(cert);
  return result;
}

bool socket_helpers::is_valid_peer_principal(const std::string &cn) {
  if (cn.empty() || cn.size() > max_peer_principal_length) return false;
  for (const char c : cn) {
    const auto uc = static_cast<unsigned char>(c);
    // Control characters (NUL, CR, LF, tab, ...) forge log lines; ':' and
    // '=' are the separators of the policy subject and of INI keys.
    if (uc < 0x20 || uc == 0x7F) return false;
    if (c == ':' || c == '=') return false;
  }
  return true;
}

std::string socket_helpers::format_subject_cn_only(void *x509) {
  if (!x509) return {};
  const auto *cert = static_cast<X509 *>(x509);
  const X509_NAME *subject = X509_get_subject_name(cert);
  if (!subject) return {};
  // Locate the LAST CN entry in the subject. RFC 5280 / standard
  // practice is to put the "most specific" CN last when there are
  // several, which matches what an operator expects when a cert has
  // an intermediate-style multi-CN structure (rare for monitoring,
  // but the convention is free). For the overwhelmingly common single-
  // CN cert, "last" and "first" are the same entry.
  int idx = -1;
  for (;;) {
    const int next = X509_NAME_get_index_by_NID(subject, NID_commonName, idx);
    if (next < 0) break;
    idx = next;
  }
  if (idx < 0) return {};
  const X509_NAME_ENTRY *entry = X509_NAME_get_entry(subject, idx);
  if (!entry) return {};
  const ASN1_STRING *data = X509_NAME_ENTRY_get_data(entry);
  if (!data) return {};
  // ASN1_STRING_to_UTF8 mallocs a NUL-terminated UTF-8 buffer; we own
  // it and must OPENSSL_free it.
  unsigned char *utf8 = nullptr;
  const int len = ASN1_STRING_to_UTF8(&utf8, data);
  if (len < 0 || !utf8) return {};
  std::string result(reinterpret_cast<const char *>(utf8), static_cast<std::size_t>(len));
  OPENSSL_free(utf8);
  return result;
}

std::string socket_helpers::extract_peer_subject_cn(void *ssl) {
  if (!ssl) return {};
  const SSL *ssl_instance = static_cast<SSL *>(ssl);
  X509 *cert = SSL_get_peer_certificate(ssl_instance);
  if (!cert) return {};
  const std::string result = format_subject_cn_only(cert);
  X509_free(cert);
  return result;
}
#endif

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

#ifdef USE_SSL
bool socket_helpers::connection_info::verifies_peer() const {
  return (ssl.get_verify_mode() & boost::asio::ssl::context_base::verify_peer) != 0;
}
#endif

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
    context.set_verify_callback([](const bool preverified, boost::asio::ssl::verify_context &v_ctx) -> bool {
      char subject_name[256] = {};
      const X509 *cert = X509_STORE_CTX_get_current_cert(v_ctx.native_handle());
      X509_NAME_oneline(X509_get_subject_name(cert), subject_name, sizeof(subject_name) - 1);
      const int error_code = X509_STORE_CTX_get_error(v_ctx.native_handle());
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
    }
    // "workarounds" and "single" are advertised under `verify mode` too, but
    // they are SSL *context* options, not verify bits: OR-ing them into the
    // value handed to SSL_CTX_set_verify did nothing useful and polluted the
    // mask (SSL_OP_ALL carries 0x4, which is SSL_VERIFY_CLIENT_ONCE). They
    // are honoured by get_ctx_opts() instead, where they take effect.
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
  // Two context options have always been documented under `verify mode`
  // rather than `ssl options` (see socket_settings_helper.hpp). Honour them
  // from there so an existing configuration keeps working - but as context
  // options, which is what they are.
  for (const std::string &key : str::utils::split_lst(verify_mode, std::string(","))) {
    if (key == "workarounds") opts |= boost::asio::ssl::context::default_workarounds;
    if (key == "single") opts |= boost::asio::ssl::context::single_dh_use;
  }
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
  std::string val(value);
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
  X509_EXTENSION *ex = X509V3_EXT_conf_nid(nullptr, &ctx, nid, val.c_str());
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

// Subject alternative names for a generated certificate.
//
// This used to be the constant "DNS:localhost,IP:127.0.0.1", which made a
// generated certificate unusable for anyone who turned peer verification on:
// ssl_connection::connect installs host_name_verification whenever
// verify_peer is set, so the certificate could only ever verify against
// localhost. Include the machine's own name (and the address a remote peer
// would see it as) so that `verify mode = peer` against a generated
// certificate is at least possible.
std::string build_subject_alt_name() {
  std::vector<std::string> names;
  // The SAN value is an OpenSSL config string, so only feed it characters
  // that cannot change its meaning - a host name is not attacker-controlled,
  // but it does come from the machine's configuration.
  const auto is_safe = [](const std::string &value) {
    if (value.empty()) return false;
    for (const char c : value) {
      if (!(isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_')) return false;
    }
    return true;
  };
  try {
    const std::string host = ip::host_name();
    if (is_safe(host)) names.push_back("DNS:" + host);
    const std::string lower = boost::algorithm::to_lower_copy(host);
    if (lower != host && is_safe(lower)) names.push_back("DNS:" + lower);
  } catch (const std::exception &) {
    // No host name: the loopback entries below still make the certificate
    // usable for a local check.
  }
  for (const bool ipv6 : {false, true}) {
    const boost::optional<ip::address> address = discover_local_address(ipv6);
    if (address) names.push_back("IP:" + address->to_string());
  }
  names.emplace_back("DNS:localhost");
  names.emplace_back("IP:127.0.0.1");
  return boost::algorithm::join(names, ",");
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
  add_ext(cert.get(), NID_subject_alt_name, build_subject_alt_name().c_str());

  if (ca) {
    add_ext(cert.get(), NID_basic_constraints, "critical,CA:TRUE");
    add_ext(cert.get(), NID_key_usage, "critical,keyCertSign,cRLSign");
    add_ext(cert.get(), NID_netscape_cert_type, "sslCA");
  }

  if (X509_sign(cert.get(), pkey.get(), EVP_sha256()) == 0) {
    throw socket_helpers::socket_exception("Failed to sign certificate: " + get_open_ssl_error());
  }
}

#ifdef WIN32
bool socket_helpers::restrict_to_owner(const std::string &path, std::list<std::string> &errors) {
  // Well-known SIDs are built rather than parsed so a machine whose
  // Administrators group is named in another language still matches.
  std::vector<unsigned char> system_sid_bytes(SECURITY_MAX_SID_SIZE, 0);
  std::vector<unsigned char> admin_sid_bytes(SECURITY_MAX_SID_SIZE, 0);
  DWORD size = SECURITY_MAX_SID_SIZE;
  if (!::CreateWellKnownSid(WinLocalSystemSid, nullptr, system_sid_bytes.data(), &size)) {
    errors.emplace_back("CreateWellKnownSid(system) failed: GetLastError=" + std::to_string(::GetLastError()));
    return false;
  }
  size = SECURITY_MAX_SID_SIZE;
  if (!::CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin_sid_bytes.data(), &size)) {
    errors.emplace_back("CreateWellKnownSid(administrators) failed: GetLastError=" + std::to_string(::GetLastError()));
    return false;
  }

  EXPLICIT_ACCESS_W access[2] = {};
  for (int i = 0; i < 2; i++) {
    access[i].grfAccessPermissions = GENERIC_ALL;
    access[i].grfAccessMode = SET_ACCESS;
    access[i].grfInheritance = NO_INHERITANCE;
    access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access[i].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  }
  access[0].Trustee.ptstrName = reinterpret_cast<LPWSTR>(system_sid_bytes.data());
  access[1].Trustee.ptstrName = reinterpret_cast<LPWSTR>(admin_sid_bytes.data());

  PACL raw_acl = nullptr;
  if (::SetEntriesInAclW(2, access, nullptr, &raw_acl) != ERROR_SUCCESS) {
    errors.emplace_back("SetEntriesInAcl failed: GetLastError=" + std::to_string(::GetLastError()));
    return false;
  }

  // PROTECTED_DACL_SECURITY_INFORMATION is what breaks inheritance. Without
  // it the "Users: Read & Execute" a file under Program Files inherits
  // survives beside the two ACEs above and the key stays world-readable.
  const std::wstring wide = utf8::cvt<std::wstring>(path);
  const DWORD result = ::SetNamedSecurityInfoW(const_cast<LPWSTR>(wide.c_str()), SE_FILE_OBJECT,
                                               DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, raw_acl, nullptr);
  ::LocalFree(raw_acl);
  if (result != ERROR_SUCCESS) {
    errors.emplace_back("SetNamedSecurityInfo failed: error=" + std::to_string(result));
    return false;
  }
  return true;
}
#endif

namespace {
// Serialize an in-memory BIO and hand back its bytes.
std::string drain_bio(BIO *bio, const std::string &what) {
  const std::size_t size = BIO_ctrl_pending(bio);
  std::string result(size, '\0');
  if (size > 0 && BIO_read(bio, &result[0], static_cast<int>(size)) < 0) {
    throw socket_helpers::socket_exception("Failed to read serialized " + what);
  }
  return result;
}

// Write `content` to `path` so that only the account running the agent (and
// the local administrators) can read it back.
//
// The plain fopen(path, "wb") this replaces left the file at 0666 & ~umask -
// 0644 under a normal systemd unit - and what goes into it is an
// *unencrypted* PKCS#8 private key. Since a default NRPE server start
// generates the file when it is missing, every default install published its
// TLS key to every local account: enough to decrypt captured traffic or
// impersonate the agent.
void write_private_file(const std::string &path, const std::string &content) {
#ifdef WIN32
  // On Windows the mode bits do nothing; the DACL is what matters. Create the
  // file empty, lock it down, and only then write the key into it, so a
  // failure to restrict never leaves a readable key behind.
  FILE *file = nullptr;
#ifdef _MSC_VER
  if (fopen_s(&file, path.c_str(), "wb") != 0) file = nullptr;
#else
  file = fopen(path.c_str(), "wb");
#endif
  if (file == nullptr) throw socket_helpers::socket_exception("Failed to write: " + path);
  fclose(file);

  std::list<std::string> errors;
  if (!socket_helpers::restrict_to_owner(path, errors)) {
    boost::system::error_code ignored;
    boost::filesystem::remove(path, ignored);
    std::string reason = errors.empty() ? std::string("unknown error") : errors.front();
    throw socket_helpers::socket_exception("Refusing to write an unprotected private key to " + path + ": " + reason);
  }
#ifdef _MSC_VER
  if (fopen_s(&file, path.c_str(), "wb") != 0) file = nullptr;
#else
  file = fopen(path.c_str(), "wb");
#endif
  if (file == nullptr) throw socket_helpers::socket_exception("Failed to write: " + path);
#else
  // O_CREAT only applies the mode to a file it creates, so narrow an existing
  // one explicitly rather than inheriting whatever it had.
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd < 0) throw socket_helpers::socket_exception("Failed to write: " + path);
  if (::fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
    ::close(fd);
    throw socket_helpers::socket_exception("Failed to restrict permissions on: " + path);
  }
  FILE *file = ::fdopen(fd, "wb");
  if (file == nullptr) {
    ::close(fd);
    throw socket_helpers::socket_exception("Failed to write: " + path);
  }
#endif
  const std::size_t written = fwrite(content.data(), sizeof(char), content.size(), file);
  fclose(file);
  if (written != content.size()) throw socket_helpers::socket_exception("Failed to write: " + path);
}

void write_public_file(const std::string &path, const std::string &content) {
  FILE *file = nullptr;
#ifdef _MSC_VER
  if (fopen_s(&file, path.c_str(), "wb") != 0) file = nullptr;
#else
  file = fopen(path.c_str(), "wb");
#endif
  if (file == nullptr) throw socket_helpers::socket_exception("Failed to write certificate to: " + path);
  const std::size_t written = fwrite(content.data(), sizeof(char), content.size(), file);
  fclose(file);
  if (written != content.size()) throw socket_helpers::socket_exception("Failed to write certificate to: " + path);
}
}  // namespace

std::string socket_helpers::ca_key_path(const std::string &ca_certificate) {
  boost::filesystem::path path(ca_certificate);
  const std::string stem = path.stem().string();
  const std::string extension = path.extension().string();
  return (path.parent_path() / (stem + "-key" + extension)).string();
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

  const BIO_ptr key_bio(BIO_new(BIO_s_mem()), BIO_free);
  if (!PEM_write_bio_PKCS8PrivateKey(key_bio.get(), private_key_instance.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
    throw socket_exception("Failed to serialize key to " + cert);
  }
  const BIO_ptr cert_bio(BIO_new(BIO_s_mem()), BIO_free);
  if (!PEM_write_bio_X509(cert_bio.get(), certificate_instance.get())) {
    throw socket_exception("Failed to serialize certificate to " + cert);
  }
  const std::string key_pem = drain_bio(key_bio.get(), "key");
  const std::string cert_pem = drain_bio(cert_bio.get(), "certificate");

  if (ca) {
    // The CA file is the one `nscp nrpe install` tells the operator to hand
    // out ("the clients need to have a certificate issued from ..."), so it
    // must contain the certificate and nothing else. It used to carry the CA
    // *private key* as well: anyone who received it could mint client
    // certificates and walk straight through `verify mode = peer-cert`, which
    // is NRPE's only real authentication. The key goes to a private sibling
    // file so an operator can still issue certificates from it.
    write_private_file(ca_key_path(cert), key_pem);
    write_public_file(cert, cert_pem);
    return;
  }
  // The agent's own identity: asio loads the key from the certificate file
  // when `certificate key` is empty, so the two stay together - in a file
  // only we can read.
  write_private_file(cert, key_pem + cert_pem);
}

boost::asio::ssl::context_base::method socket_helpers::tls_method_parser(const std::string &tls_version) {
  const std::string tmp = boost::algorithm::to_lower_copy(tls_version);
  // A '+' form ("1.2+") means "this version or later". The version-pinned
  // methods (context::tlsv12, ...) pin BOTH the minimum and the maximum
  // protocol version, so returning one of them here silently excluded every
  // later version - "1.2+" negotiated TLS 1.2 only and never 1.3. The generic
  // method negotiates the highest mutually supported version; the floor is
  // applied separately via apply_tls_min_version() because a method cannot
  // carry it.
  if (!tmp.empty() && tmp.back() == '+') {
    // Validate the version part so a typo ("1.4+") still fails loudly here
    // rather than surfacing as a floor-less "any".
    tls_min_version_parser(tls_version);
    return boost::asio::ssl::context::tls;
  }
  // Documented alongside the numeric versions: no pin and no floor, accept
  // whatever both sides support.
  if (tmp == "any") {
    return boost::asio::ssl::context::tls;
  }
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

long socket_helpers::tls_min_version_parser(const std::string &tls_version) {
  std::string tmp = boost::algorithm::to_lower_copy(tls_version);
  if (tmp.empty() || tmp.back() != '+') return 0;
  tmp.pop_back();
  if (tmp == "tlsv1.3" || tmp == "tls1.3" || tmp == "1.3") return TLS1_3_VERSION;
  if (tmp == "tlsv1.2" || tmp == "tls1.2" || tmp == "1.2") return TLS1_2_VERSION;
  if (tmp == "tlsv1.1" || tmp == "tls1.1" || tmp == "1.1") return TLS1_1_VERSION;
  if (tmp == "tlsv1.0" || tmp == "tls1.0" || tmp == "1.0") return TLS1_VERSION;
  throw socket_exception("Invalid tls version: " + tls_version);
}

void socket_helpers::apply_tls_min_version(boost::asio::ssl::context &ctx, const std::string &tls_version) {
  const long min_version = tls_min_version_parser(tls_version);
  if (min_version == 0) return;
  if (SSL_CTX_set_min_proto_version(ctx.native_handle(), min_version) != 1) {
    throw socket_exception("Failed to set the minimum TLS version for: " + tls_version);
  }
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

boost::optional<long> socket_helpers::peer_certificate_expiry_days(SSL *ssl) {
  if (ssl == nullptr) return boost::none;
  X509 *cert = SSL_get_peer_certificate(ssl);
  if (cert == nullptr) return boost::none;

  // ASN1_TIME_diff splits the interval into whole days plus leftover seconds,
  // both carrying the sign of the interval. We report whole days and drop the
  // remainder, so a certificate with 23 hours left reads as 0 rather than
  // rounding up to a reassuring 1.
  //
  // Dropping the remainder has to round DOWN, not toward zero. A certificate
  // that expired three hours ago comes back as days=0, seconds=-10800: taking
  // days as-is reports 0, which is indistinguishable from "23 hours left" and
  // leaves an already-expired certificate looking merely urgent. Any threshold
  // written the obvious way ("critical when < 0") would never fire during the
  // first day of expiry. Flooring keeps the useful invariant that the value is
  // negative if and only if the certificate has expired, and never reports more
  // time than actually remains.
  int days = 0;
  int seconds = 0;
  const int ok = ASN1_TIME_diff(&days, &seconds, nullptr, X509_get0_notAfter(cert));
  X509_free(cert);
  if (ok != 1) return boost::none;
  if (seconds < 0) days -= 1;
  return static_cast<long>(days);
}

#endif
