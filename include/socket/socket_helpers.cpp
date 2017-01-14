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

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>
#include <utf8.hpp>

#include <socket/socket_helpers.hpp>
#ifndef WIN32
#include <openssl/x509v3.h>
#endif
const int socket_helpers::connection_info::backlog_default = 0;

namespace ip = boost::asio::ip;

std::list<std::string> socket_helpers::connection_info::validate() {
	return validate_ssl();
}
void socket_helpers::validate_certificate(const std::string &certificate, std::list<std::string> &list) {
#ifdef USE_SSL
	if (!certificate.empty() && !boost::filesystem::is_regular(certificate)) {
		if (boost::algorithm::ends_with(certificate, "/certificate.pem")) {
			list.push_back("Certificate not found: " + certificate + " (generating a default certificate)");
			write_certs(certificate, false);
		} else if (boost::algorithm::ends_with(certificate, "/ca.pem")) {
			list.push_back("CA not found: " + certificate + " (generating a default CA)");
			write_certs(certificate, true);
		} else
			list.push_back("Certificate not found: " + certificate);
	}
#else
	list.push_back("SSL is not supported (not compiled with openssl)");
#endif
}

std::list<std::string> socket_helpers::connection_info::validate_ssl() {
	std::list<std::string> list;
	if (!ssl.enabled)
		return list;
#ifndef USE_SSL
	list.push_back("SSL is not supported (not compiled with openssl)");
#endif

#ifdef USE_SSL
	validate_certificate(ssl.certificate, list);
	validate_certificate(ssl.ca_path, list);
	if (!ssl.certificate_key.empty() && !boost::filesystem::is_regular(ssl.certificate_key))
		list.push_back("Certificate key not found: " + ssl.certificate_key);
	if (!ssl.dh_key.empty() && !boost::filesystem::is_regular(ssl.dh_key))
		list.push_back("DH key not found: " + ssl.dh_key);
#endif
	return list;
}

long socket_helpers::connection_info::get_ctx_opts() {
	long opts = 0;
#ifdef USE_SSL
	opts |= ssl.get_ctx_opts();
#endif
	return opts;
}

std::string socket_helpers::allowed_hosts_manager::to_string() {
	std::string ret;
	BOOST_FOREACH(const host_record_v4 &r, entries_v4) {
		ip::address_v4 a(r.addr);
		ip::address_v4 m(r.mask);
		std::string s = a.to_string() + "(" + m.to_string() + ")";
		str::format::append_list(ret, s);
	}
	BOOST_FOREACH(const host_record_v6 &r, entries_v6) {
		ip::address_v6 a(r.addr);
		ip::address_v6 m(r.mask);
		std::string s = a.to_string() + "(" + m.to_string() + ")";
		str::format::append_list(ret, s);
	}
	return ret;
}

std::size_t extract_mask(std::string &mask, std::size_t masklen) {
	if (!mask.empty()) {
		std::string::size_type p1 = mask.find_first_of("0123456789");
		if (p1 != std::string::npos) {
			std::string::size_type p2 = mask.find_first_not_of("0123456789", p1);
			if (p2 != std::string::npos)
				masklen = str::stox<std::size_t>(mask.substr(p1, p2));
			else
				masklen = str::stox<std::size_t>(mask.substr(p1));
		}
	}
	return static_cast<unsigned int>(masklen);
}

template<class addr>
addr calculate_mask(std::string mask_s) {
	addr ret;
	const std::size_t byte_size = 8;
	const std::size_t largest_byte = 0xff;
	const std::size_t mask = extract_mask(mask_s, byte_size*ret.size());
	std::size_t index = mask / byte_size;
	std::size_t reminder = mask % byte_size;

	std::size_t value = largest_byte - (largest_byte >> reminder);

	for (std::size_t i = 0; i < ret.size(); i++) {
		if (i < index)
			ret[i] = largest_byte;
		else if (i == index)
			ret[i] = static_cast<unsigned char>(value);
		else
			ret[i] = 0;
	}
	return ret;
}

void socket_helpers::allowed_hosts_manager::set_source(std::string source) {
	sources.clear();
	BOOST_FOREACH(std::string s, str::utils::split_lst(source, std::string(","))) {
		boost::trim(s);
		if (!s.empty())
			sources.push_back(s);
	}
}

void socket_helpers::allowed_hosts_manager::refresh(std::list<std::string> &errors) {
	boost::asio::io_service io_service;
	ip::tcp::resolver resolver(io_service);
	entries_v4.clear();
	entries_v6.clear();
	BOOST_FOREACH(const std::string &record, sources) {
		std::string::size_type pos = record.find('/');
		std::string addr, mask;
		if (pos == std::string::npos) {
			addr = record;
			mask = "";
		} else {
			addr = record.substr(0, pos);
			mask = record.substr(pos);
		}
		if (addr.empty())
			continue;

		if (std::isdigit(addr[0])) {
			ip::address a = ip::address::from_string(addr);
			if (a.is_v4()) {
				entries_v4.push_back(host_record_v4(record, a.to_v4().to_bytes(), calculate_mask<addr_v4>(mask)));
			} else if (a.is_v6()) {
				entries_v6.push_back(host_record_v6(record, a.to_v6().to_bytes(), calculate_mask<addr_v6>(mask)));
			} else {
				errors.push_back("Invalid address: " + record);
			}
		} else {
			try {
				ip::tcp::resolver::query query(addr, "");
				ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
				ip::tcp::resolver::iterator end;
				for (; endpoint_iterator != end; ++endpoint_iterator) {
					ip::address a = endpoint_iterator->endpoint().address();
					if (a.is_v4()) {
						entries_v4.push_back(host_record_v4(record, a.to_v4().to_bytes(), calculate_mask<addr_v4>(mask)));
					} else if (a.is_v6()) {
						entries_v6.push_back(host_record_v6(record, a.to_v6().to_bytes(), calculate_mask<addr_v6>(mask)));
					} else {
						errors.push_back("Invalid address: " + record);
					}
				}
			} catch (const std::exception &e) {
				errors.push_back("Failed to parse host " + record + ": " + utf8::utf8_from_native(e.what()));
			}
		}
	}
}

void socket_helpers::io::set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b) {
	if (!b) {
		a->reset(b);
	}
}
#ifdef USE_SSL
void socket_helpers::connection_info::ssl_opts::configure_ssl_context(boost::asio::ssl::context &context, std::list<std::string> &errors) const {
	boost::system::error_code er;
	if (!certificate.empty() && certificate != "none") {
		context.use_certificate_chain_file(certificate,  er);
		if (er)
			errors.push_back("Failed to load certificate " + certificate + ": " + utf8::utf8_from_native(er.message()));
		if (!certificate_key.empty() && certificate_key != "none") {
			context.use_private_key_file(certificate_key, get_certificate_key_format(), er);
			if (er)
				errors.push_back("Failed to load certificate key " + certificate_key + ": " + utf8::utf8_from_native(er.message()));
		} else {
			context.use_private_key_file(certificate, get_certificate_key_format(), er);
			if (er)
				errors.push_back("Failed to load certificate (as key) " + certificate + ": " + utf8::utf8_from_native(er.message()));
		}
	}
	context.set_verify_mode(get_verify_mode(), er);
	if (er)
		errors.push_back("Failed to set verify mode: " + utf8::utf8_from_native(er.message()));
	if (!allowed_ciphers.empty())
		SSL_CTX_set_cipher_list(context.impl(), allowed_ciphers.c_str());
	if (!dh_key.empty() && dh_key != "none") {
		context.use_tmp_dh_file(dh_key, er);
		if (er)
			errors.push_back("Failed to set dh file " + dh_key + ": " + utf8::utf8_from_native(er.message()));
	}

	if (!ca_path.empty()) {
		context.load_verify_file(ca_path, er);
		if (er)
			errors.push_back("Failed to load CA " + ca_path + ": " + utf8::utf8_from_native(er.message()));
	}
}

boost::asio::ssl::context::verify_mode socket_helpers::connection_info::ssl_opts::get_verify_mode() const {
	boost::asio::ssl::context::verify_mode mode = boost::asio::ssl::context_base::verify_none;
	BOOST_FOREACH(const std::string &key, str::utils::split_lst(verify_mode, std::string(","))) {
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

boost::asio::ssl::context::file_format socket_helpers::connection_info::ssl_opts::get_certificate_format() const {
	if (certificate_format == "asn1")
		return boost::asio::ssl::context::asn1;
	return boost::asio::ssl::context::pem;
}

boost::asio::ssl::context::file_format socket_helpers::connection_info::ssl_opts::get_certificate_key_format() const {
	if (certificate_key_format == "asn1")
		return boost::asio::ssl::context::asn1;
	return boost::asio::ssl::context::pem;
}
#ifdef USE_SSL
long socket_helpers::connection_info::ssl_opts::get_ctx_opts() const {
	long opts = 0;
	BOOST_FOREACH(const std::string &key, str::utils::split_lst(ssl_options, std::string(","))) {
		if (key == "default-workarounds")
			opts |= boost::asio::ssl::context::default_workarounds;
		if (key == "no-sslv2")
			opts |= boost::asio::ssl::context::no_sslv2;
		if (key == "no-sslv3")
			opts |= boost::asio::ssl::context::no_sslv3;
		if (key == "no-tlsv1")
			opts |= boost::asio::ssl::context::no_tlsv1;
		if (key == "single-dh-use")
			opts |= boost::asio::ssl::context::single_dh_use;
	}
	return opts;
}
#endif

void genkey_callback(int, int, void*) {
	// Ignored as we dont want to show progress...
}

int add_ext(X509 *cert, int nid, const char *value) {
	std::size_t len = strlen(value);
	char *tmp = new char[len + 10];
	strncpy(tmp, value, len);
	X509_EXTENSION *ex;
	X509V3_CTX ctx;
	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
	ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, tmp);
	delete[] tmp;
	if (!ex)
		return 0;
	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);
	return 1;
}
void make_certificate(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days, bool ca) {
	X509 *x;
	EVP_PKEY *pk;
	RSA *rsa;
	X509_NAME *name = NULL;
	if ((pkeyp == NULL) || (*pkeyp == NULL)) {
		if ((pk = EVP_PKEY_new()) == NULL)
			throw socket_helpers::socket_exception("Failed to create private key");
	} else
		pk = *pkeyp;

	if ((x509p == NULL) || (*x509p == NULL)) {
		if ((x = X509_new()) == NULL)
			throw socket_helpers::socket_exception("Failed to create certificate");
	} else
		x = *x509p;

	rsa = RSA_generate_key(bits, RSA_F4, genkey_callback, NULL);
	if (!EVP_PKEY_assign_RSA(pk, rsa))
		throw socket_helpers::socket_exception("Failed to assign RSA data");
	rsa = NULL;

	X509_set_version(x, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(x), serial);
	X509_gmtime_adj(X509_get_notBefore(x), 0);
	X509_gmtime_adj(X509_get_notAfter(x), (long)60 * 60 * 24 * days);
	X509_set_pubkey(x, pk);

	name = X509_get_subject_name(x);

	//X509_NAME_add_entry_by_txt(name,"C", MBSTRING_ASC, (unsigned char*)"NA", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);

	X509_set_issuer_name(x, name);

	if (ca) {
		add_ext(x, NID_basic_constraints, "critical,CA:TRUE");
		add_ext(x, NID_key_usage, "critical,keyCertSign,cRLSign");
		add_ext(x, NID_subject_key_identifier, "hash");
		add_ext(x, NID_netscape_cert_type, "sslCA");
		add_ext(x, NID_netscape_comment, "example comment extension");
	}

	if (!X509_sign(x, pk, EVP_sha1()))
		throw socket_helpers::socket_exception("Failed to sign certificate");

	*x509p = x;
	*pkeyp = pk;
}

void socket_helpers::write_certs(std::string cert, bool ca) {
	X509 *x509 = NULL;
	EVP_PKEY *pkey = NULL;

	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

	make_certificate(&x509, &pkey, 2048, 0, 365, ca);

	BIO *bio = BIO_new(BIO_s_mem());
	PEM_write_bio_PKCS8PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL);
	PEM_write_bio_X509(bio, x509);

	std::size_t size = BIO_ctrl_pending(bio);
	char * buf = new char[size];
	if (BIO_read(bio, buf, size) < 0) {
		throw socket_helpers::socket_exception("Failed to write key");
	}

	BIO_free(bio);

	FILE *fout = fopen(cert.c_str(), "wb");
	if (fout == NULL)
		throw socket_helpers::socket_exception("Failed to open file: " + cert);
	fwrite(buf, sizeof(char), size, fout);
	fclose(fout);

	X509_free(x509);
	EVP_PKEY_free(pkey);

#ifndef OPENSSL_NO_ENGINE
	ENGINE_cleanup();
#endif
	CRYPTO_cleanup_all_ex_data();
}

#endif