#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <strEx.h>
#include <socket/socket_helpers.hpp>


const int socket_helpers::connection_info::backlog_default = 0;

namespace ip = boost::asio::ip;

std::list<std::wstring> socket_helpers::connection_info::validate() {
	return validate_ssl();
}

std::list<std::wstring> socket_helpers::connection_info::validate_ssl() {
	std::list<std::wstring> list;
	if (!ssl.enabled)
		return list;
#ifndef USE_SSL
	list.push_back(_T("SSL is not supported (not compiled with openssl)"));
#endif

	if (!ssl.certificate.empty() && !boost::filesystem::is_regular(ssl.certificate))
		list.push_back(_T("Certificate not found: ") + utf8::cvt<std::wstring>(ssl.certificate));
	if (!ssl.certificate_key.empty() && !boost::filesystem::is_regular(ssl.certificate_key))
		list.push_back(_T("Certificate key not found: ") + utf8::cvt<std::wstring>(ssl.certificate_key));
	if (!ssl.dh_key.empty() && !boost::filesystem::is_regular(ssl.dh_key))
		list.push_back(_T("DH key not found: ") + utf8::cvt<std::wstring>(ssl.dh_key));
	return list;
}

std::wstring socket_helpers::allowed_hosts_manager::to_wstring() {
	std::wstring ret;
	BOOST_FOREACH(const host_record_v4 &r, entries_v4) {
		ip::address_v4 a(r.addr);
		ip::address_v4 m(r.mask);
		std::wstring s = utf8::cvt<std::wstring>(a.to_string()) + _T("(") + utf8::cvt<std::wstring>(m.to_string()) + _T(")");
		strEx::append_list(ret, s);
	}
	BOOST_FOREACH(const host_record_v6 &r, entries_v6) {
		ip::address_v6 a(r.addr);
		ip::address_v6 m(r.mask);
		std::wstring s = utf8::cvt<std::wstring>(a.to_string()) + _T("(") + utf8::cvt<std::wstring>(m.to_string()) + _T(")");
		strEx::append_list(ret, s);
	}
	return ret;
}

unsigned int extract_mask(std::string &mask, unsigned int masklen) {
	if (!mask.empty()) {
		std::string::size_type p1 = mask.find_first_of("0123456789");
		if (p1 != std::wstring::npos) {
			std::string::size_type p2 = mask.find_first_not_of("0123456789", p1);
			if (p2 != std::wstring::npos)
				masklen = strEx::stoi(mask.substr(p1, p2));
			else
				masklen = strEx::stoi(mask.substr(p1));
		}
	}
	return masklen;
}

template<class addr>
addr calculate_mask(std::string mask_s) {
	addr ret;
	const unsigned int byte_size = 8;
	const unsigned int largest_byte = 0xff;
	unsigned int mask = extract_mask(mask_s, byte_size*ret.size());
	unsigned int index = mask / byte_size;
	unsigned int reminder = mask % byte_size;

	unsigned int value = largest_byte - (largest_byte >> reminder);

	for (unsigned int i=0;i<ret.size();i++) {
		if (i < index)
			ret[i] = largest_byte;
		else if (i == index)
			ret[i] = value;
		else
			ret[i] = 0;
	}
	return ret;
}

void socket_helpers::allowed_hosts_manager::refresh(std::list<std::string> &errors) {
	boost::asio::io_service io_service;
	ip::tcp::resolver resolver(io_service);
	entries_v4.clear();
	entries_v6.clear();
	BOOST_FOREACH(std::string &record, sources) {
		boost::trim(record);
		if (record.empty())
			continue;
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
