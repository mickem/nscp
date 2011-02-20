#pragma once
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>

namespace socketHelpers {
	class allowedHosts {
		struct host_record {
			host_record() : mask(0) {}
			host_record(std::wstring r) : mask(0), record(r) {}
			std::wstring record;
			std::wstring host;
			u_long in_addr;
			unsigned long mask;
		};
	public:
		typedef std::list<host_record> host_list; 
		typedef std::list<std::wstring> string_list; 
	private:
		host_list allowed_list_;
		string_list lookup_list;
		bool cachedAddresses_;
	public:
		allowedHosts() : cachedAddresses_(true) {}

		unsigned int lookupMask(std::wstring mask) {
			unsigned int masklen = 32;
			if (!mask.empty()) {
				std::wstring::size_type pos = mask.find_first_of(_T("0123456789"));
				if (pos != std::wstring::npos) {
					masklen = strEx::stoi(mask.substr(pos));
				}
			}
			if (masklen > 32)
				masklen = 32;
			return (~((unsigned int)0))>>(32-masklen);
		}
		void lookupList(boost::asio::io_service& io_service) {
			allowed_list_.clear();
			for (string_list::iterator it = lookup_list.begin();it!=lookup_list.end();++it) {
				std::wstring host = (*it);
				host_record tmp_record;
				if (!host.empty()) {
					try {
						std::wstring::size_type pos = host.find('/');
						if (pos == std::wstring::npos) {
							tmp_record.host = host;
							tmp_record.mask = lookupMask(_T(""));
						} else {
							tmp_record.host = host.substr(0, pos);
							tmp_record.mask = lookupMask(host.substr(pos));
						}
						boost::asio::ip::tcp::resolver resolver(io_service);
						boost::asio::ip::tcp::resolver::query query(::to_string(tmp_record.host), "");
						boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
						boost::asio::ip::tcp::resolver::iterator end;
						for (;endpoint_iterator != end; ++endpoint_iterator) {
							tmp_record.in_addr = endpoint_iterator->endpoint().address().to_v4().to_ulong();
							tmp_record.host = to_wstring(endpoint_iterator->endpoint().address().to_string());
							allowed_list_.push_back(tmp_record);
						}
						/*
						std::cerr << "Added: " 
							+ simpleSocket::Socket::inet_ntoa((*it).in_addr)
							+ " with mask "
							+ simpleSocket::Socket::inet_ntoa((*it).mask)
							+ " from "
							+ (*it).record <<
							std::endl;
							*/
					} catch (std::exception &e) {
						std::cerr << "Filed to lookup host: " << e.what() << std::endl;
					} catch (...) {
						std::wcerr << _T("Filed to lookup host: ") << std::endl;
					}
				}
			}
		}

		void setAllowedHosts(const std::list<std::wstring> list, bool cachedAddresses, boost::asio::io_service& io_service) {
			for (std::list<std::wstring>::const_iterator it = list.begin(); it != list.end(); ++it) {
				if (!(*it).empty())
					lookup_list.push_back(*it);
			}
			cachedAddresses_ = cachedAddresses;
			lookupList(io_service);
		}
		bool matchHost(host_record allowed, struct in_addr remote) {
			/*
			if ((allowed.in_addr&allowed.mask)==(remote.S_un.S_addr&allowed.mask)) {
				std::cerr << "Matched: " << simpleSocket::Socket::inet_ntoa(allowed.in_addr)  << " with " << 
					simpleSocket::Socket::inet_ntoa(remote.S_un.S_addr) << std::endl;
			}
			*/
			return true; //((allowed.in_addr&allowed.mask)==(remote.S_un.S_addr&allowed.mask));
		}
		bool inAllowedHosts(boost::asio::io_service& io_service, struct in_addr remote) {
			if (lookup_list.empty())
				return true;
			if (!cachedAddresses_) {
				lookupList(io_service);
			}
			for (host_list::const_iterator cit = allowed_list_.begin();cit!=allowed_list_.end();++cit) {
				if (matchHost((*cit), remote))
					return true;
			}
			return false;
		}
		std::wstring to_string() {
			std::wstring ret;
			BOOST_FOREACH(host_record r, allowed_list_) {
				if (!ret.empty()) ret += _T(", ");
				ret += r.host;
			}
			return ret;
		}
	};

	namespace io {
		void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b) {
			a->reset(b);
		} 

		template <typename AsyncReadStream, typename RawSocket, typename MutableBufferSequence>
		void read_with_timeout(AsyncReadStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
			boost::optional<boost::system::error_code> timer_result;
			boost::asio::deadline_timer timer(sock.io_service());
			timer.expires_from_now(duration);
			timer.async_wait(boost::bind(set_result, &timer_result, _1));

			boost::optional<boost::system::error_code> read_result;
			async_read(sock, buffers, boost::bind(set_result, &read_result, _1));

			sock.io_service().reset();
			while (sock.io_service().run_one()) {
				if (read_result)
					timer.cancel();
				else if (timer_result)
					rawSocket.close();
			}

			if (*read_result)
				throw boost::system::system_error(*read_result);
		} 

		template <typename AsyncWriteStream, typename RawSocket, typename MutableBufferSequence>
		void write_with_timeout(AsyncWriteStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
			boost::optional<boost::system::error_code> timer_result;
			boost::asio::deadline_timer timer(sock.io_service());
			timer.expires_from_now(duration);
			timer.async_wait(boost::bind(set_result, &timer_result, _1));

			boost::optional<boost::system::error_code> read_result;
			async_write(sock, buffers, boost::bind(set_result, &read_result, _1));

			sock.io_service().reset();
			while (sock.io_service().run_one()) {
				if (read_result)
					timer.cancel();
				else if (timer_result)
					rawSocket.close();
			}

			if (*read_result)
				throw boost::system::system_error(*read_result);
		}

	}
}

