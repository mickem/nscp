#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

#include <strEx.h>
#include <socket/socket_helpers.hpp>


const int socket_helpers::connection_info::backlog_default = 0;

namespace ip = boost::asio::ip;


std::wstring socket_helpers::allowed_hosts_manager::to_wstring() {
	std::wstring ret;
	BOOST_FOREACH(const host_record &r, entries) {
		ip::address_v4 a(r.in_addr);
		ip::address_v4 m(r.mask);
		std::wstring s = utf8::cvt<std::wstring>(a.to_string()) + _T("(") + utf8::cvt<std::wstring>(m.to_string()) + _T(")");
		strEx::append_list(ret, s);
	}
	return ret;
}

unsigned int socket_helpers::allowed_hosts_manager::lookup_mask(std::string mask) {
	unsigned int masklen = 32;
	if (!mask.empty()) {
		std::string::size_type pos = mask.find_first_of("0123456789");
		if (pos != std::wstring::npos) {
			masklen = strEx::stoi(mask.substr(pos));
		}
	}
	if (masklen > 32)
		masklen = 32;
	return (0xffffffff << (32 - masklen )) & 0xffffffff;
}

void socket_helpers::allowed_hosts_manager::refresh(std::list<std::string> &errors) {
	boost::asio::io_service io_service;
	ip::tcp::resolver resolver(io_service);
	entries.clear();
	host_record tmp_record;
	BOOST_FOREACH(std::string &record, sources) {
		boost::trim(record);
		if (!record.empty()) {
			std::string::size_type pos = record.find('/');
			if (pos == std::string::npos) {
				tmp_record.host = record;
				tmp_record.mask = lookup_mask("");
			} else {
				tmp_record.host = record.substr(0, pos);
				tmp_record.mask = lookup_mask(record.substr(pos));
			}

			if (std::isdigit(tmp_record.host[0])) {
				ip::address_v4 a = ip::address_v4::from_string(tmp_record.host);
				tmp_record.in_addr = a.to_ulong();
				entries.push_back(tmp_record);
			} else {
				try {
					ip::tcp::resolver::query query(tmp_record.host, "");
					ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
					ip::tcp::resolver::iterator end;
					for (;endpoint_iterator != end; ++endpoint_iterator) {
						tmp_record.in_addr = endpoint_iterator->endpoint().address().to_v4().to_ulong();
						tmp_record.host = endpoint_iterator->endpoint().address().to_string();
						entries.push_back(tmp_record);
					}
				} catch (const std::exception &e) {
					errors.push_back("Failed to lookup allowed host " + record + ": " + e.what());
				}
			}
		}
	}
}




void socket_helpers::io::set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b) {
	if (!b) {
		a->reset(b);
	} else {
		std::cout << "timer aborted incorrectly: " << b.message() << std::endl;
	}
}
