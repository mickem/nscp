#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <strEx.h>
#include <utf8.hpp>

#include <socket/socket_helpers.hpp>


const int socket_helpers::connection_info::backlog_default = 0;

namespace ip = boost::asio::ip;

std::list<std::string> socket_helpers::connection_info::validate() {
	return validate_ssl();
}

std::list<std::string> socket_helpers::connection_info::validate_ssl() {
	std::list<std::string> list;
	if (!ssl.enabled)
		return list;
#ifndef USE_SSL
	list.push_back("SSL is not supported (not compiled with openssl)");
#endif

#ifdef USE_SSL
	if (!ssl.certificate.empty() && !boost::filesystem::is_regular(ssl.certificate)) {
		list.push_back("Certificate not found: " + ssl.certificate);
	}
	if (!ssl.certificate_key.empty() && !boost::filesystem::is_regular(ssl.certificate_key))
		list.push_back("Certificate key not found: " + ssl.certificate_key);
	if (!ssl.dh_key.empty() && !boost::filesystem::is_regular(ssl.dh_key))
		list.push_back("DH key not found: " + ssl.dh_key);
#endif
	return list;
}

// std::wstring socket_helpers::allowed_hosts_manager::to_wstring() {
// 	std::wstring ret;
// 	BOOST_FOREACH(const host_record_v4 &r, entries_v4) {
// 		ip::address_v4 a(r.addr);
// 		ip::address_v4 m(r.mask);
// 		std::wstring s = utf8::cvt<std::wstring>(a.to_string()) + _T("(") + utf8::cvt<std::wstring>(m.to_string()) + _T(")");
// 		strEx::append_list(ret, s);
// 	}
// 	BOOST_FOREACH(const host_record_v6 &r, entries_v6) {
// 		ip::address_v6 a(r.addr);
// 		ip::address_v6 m(r.mask);
// 		std::wstring s = utf8::cvt<std::wstring>(a.to_string()) + _T("(") + utf8::cvt<std::wstring>(m.to_string()) + _T(")");
// 		strEx::append_list(ret, s);
// 	}
// 	return ret;
// }
std::string socket_helpers::allowed_hosts_manager::to_string() {
	std::string ret;
	BOOST_FOREACH(const host_record_v4 &r, entries_v4) {
		ip::address_v4 a(r.addr);
		ip::address_v4 m(r.mask);
		std::string s = a.to_string() + "(" + m.to_string() + ")";
		strEx::append_list(ret, s);
	}
	BOOST_FOREACH(const host_record_v6 &r, entries_v6) {
		ip::address_v6 a(r.addr);
		ip::address_v6 m(r.mask);
		std::string s = a.to_string() + "(" + m.to_string() + ")";
		strEx::append_list(ret, s);
	}
	return ret;
}

std::size_t extract_mask(std::string &mask, std::size_t masklen) {
	if (!mask.empty()) {
		std::string::size_type p1 = mask.find_first_of("0123456789");
		if (p1 != std::string::npos) {
			std::string::size_type p2 = mask.find_first_not_of("0123456789", p1);
			if (p2 != std::string::npos)
				masklen = strEx::s::stox<std::size_t>(mask.substr(p1, p2));
			else
				masklen = strEx::s::stox<std::size_t>(mask.substr(p1));
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

	for (std::size_t i=0;i<ret.size();i++) {
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
	BOOST_FOREACH(std::string s, strEx::s::splitEx(source, std::string(","))) {
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
				for (;endpoint_iterator != end; ++endpoint_iterator) {
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
				errors.push_back("Failed to parse host " + record + ": " + e.what());
			}
		}
	}
}




void socket_helpers::io::set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b) {
	if (!b) {
		a->reset(b);
// 	} else {
// 		std::cout << "timer aborted incorrectly: " << b.message() << std::endl;
	}
}
#ifdef USE_SSL
void socket_helpers::connection_info::ssl_opts::configure_ssl_context(boost::asio::ssl::context &context, std::list<std::string> errors) {
	boost::system::error_code er;
	if (!certificate.empty() && certificate != "none") {
		context.use_certificate_file(certificate, get_certificate_format(), er);
		if (er)
			errors.push_back("Failed to load certificate " + certificate + ": " + utf8::utf8_from_native(er.message()));
		if (!certificate_key.empty() && certificate_key != "none") {
			context.use_private_key_file(certificate_key, get_certificate_key_format(), er);
			if (er)
				errors.push_back("Failed to load certificate key " + certificate_key + ": " + utf8::utf8_from_native(er.message()));
		} else {
			context.use_private_key_file(certificate, get_certificate_key_format(), er);
			if (er)
				errors.push_back("Failed to load certificate " + certificate + ": " + utf8::utf8_from_native(er.message()));
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

boost::asio::ssl::context::verify_mode socket_helpers::connection_info::ssl_opts::get_verify_mode()
{
	boost::asio::ssl::context::verify_mode mode = boost::asio::ssl::context_base::verify_none;
	BOOST_FOREACH(const std::string &key, strEx::s::splitEx(verify_mode, std::string(","))) {
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
		else if (key == "workarounds")
			mode |= boost::asio::ssl::context_base::default_workarounds;
		else if (key == "single")
			mode |= boost::asio::ssl::context::single_dh_use;
	}
	return mode;
}

boost::asio::ssl::context::file_format socket_helpers::connection_info::ssl_opts::get_certificate_format()
{
	if (certificate_format == "asn1")
		return boost::asio::ssl::context::asn1;
	return boost::asio::ssl::context::pem;
}

boost::asio::ssl::context::file_format socket_helpers::connection_info::ssl_opts::get_certificate_key_format()
{
	if (certificate_key_format == "asn1")
		return boost::asio::ssl::context::asn1;
	return boost::asio::ssl::context::pem;
}
#endif