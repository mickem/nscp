#pragma once

#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

namespace socket_helpers {

	struct allowed_hosts_manager {

		struct host_record {
			host_record() : mask(0), in_addr(0) {}
			host_record(const host_record &other) : mask(other.mask), in_addr(other.in_addr), host(other.host) {}
			const host_record& operator=(const host_record &other) {
				mask = other.mask;
				in_addr = other.in_addr;
				host = other.host;
				return *this;
			}
			std::string host;
			u_long in_addr;
			unsigned long mask;
		};

		std::list<host_record> entries;
		std::list<std::string> sources;
		//std::wstring list;
		bool cached;

		allowed_hosts_manager() : cached(true) {}
		allowed_hosts_manager(const allowed_hosts_manager &other) : entries(other.entries), sources(other.sources), cached(other.cached) {}
		const allowed_hosts_manager& operator=(const allowed_hosts_manager &other) {
			entries = other.entries;
			sources = other.sources;
			cached = other.cached;
			return *this;
		}

		void set_source(std::wstring source) {
			sources.clear();
			BOOST_FOREACH(const std::wstring &s, strEx::splitEx(source, _T(","))) {
				sources.push_back(utf8::cvt<std::string>(s));
			}
		}
		unsigned int lookup_mask(std::string mask);
		void refresh(std::list<std::string> &errors);

		inline bool match_host(const host_record &allowed, const unsigned long &remote) const {
			return ((allowed.in_addr&allowed.mask)==(remote&allowed.mask));
		}
		bool is_allowed(const unsigned long &remote, std::list<std::string> &errors) {
			if (entries.empty())
				return true;
			if (!cached)
				refresh(errors);
			BOOST_FOREACH(const host_record &r, entries) {
				if (match_host(r, remote))
					return true;
			}
			return false;
		}
		std::wstring to_wstring();
	};

	struct connection_info {
		static const int backlog_default;
		connection_info() : back_log(backlog_default), port(0), thread_pool_size(0), use_ssl(false), timeout(30) {}

		connection_info(const connection_info &other) 
			: address(other.address)
			, port(other.port)
			, thread_pool_size(other.thread_pool_size)
			, back_log(other.back_log)
			, use_ssl(other.use_ssl)
			, timeout(other.timeout)
			, certificate(other.certificate)
			, allowed_hosts(other.allowed_hosts)
			{
			}
		connection_info& operator=(const connection_info &other) {
			address = other.address;
			port = other.port;
			thread_pool_size = other.thread_pool_size;
			back_log = other.back_log;
			use_ssl = other.use_ssl;
			timeout = other.timeout;
			certificate = other.certificate;
			allowed_hosts = other.allowed_hosts;
			return *this;
		}


		std::string address;
		unsigned int port;
		unsigned int thread_pool_size;
		int back_log;
		bool use_ssl;
		unsigned int timeout;
		std::wstring certificate;

		allowed_hosts_manager allowed_hosts;

		std::string get_port() { return utf8::cvt<std::string>(strEx::itos(port)); }
		std::string get_address() { return address; }
		std::wstring get_endpoint_str() {
			return utf8::cvt<std::wstring>(address) + _T(":") + utf8::cvt<std::wstring>(get_port());
		}
	};



	namespace io {
		void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b);

		struct timed_writer : boost::noncopyable {
			boost::asio::io_service &io_service;
			boost::posix_time::time_duration duration;
			boost::asio::deadline_timer timer;

			boost::optional<boost::system::error_code> timer_result;
			boost::optional<boost::system::error_code> read_result;

			timed_writer(boost::asio::io_service &io_service, boost::posix_time::time_duration duration)
				: io_service(io_service) 
				, timer(io_service)
			{
				timer.expires_from_now(duration);
				timer.async_wait(boost::bind(set_result, &timer_result, _1));
			}
			~timed_writer() {
				timer.cancel();
			}

			template <typename AsyncWriteStream, typename MutableBufferSequence>
			void write(AsyncWriteStream& socket, MutableBufferSequence &buffer) {
				async_write(socket, buffer, boost::bind(set_result, &read_result, _1));
			}

			template <typename AsyncWriteStream, typename RawSocket, typename MutableBufferSequence>
			bool write_and_wait(AsyncWriteStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffer) {
				write(sock, buffer);
				return wait(rawSocket);
			}

			template <typename RawSocket>
			bool wait(RawSocket& socket) {
				io_service.reset();
				while (io_service.run_one()) {
					if (read_result) {
						return true;
					}
					else if (timer_result) {
						socket.close();
						return false;
					}
				}
			}
		};


		template <typename AsyncWriteStream, typename RawSocket, typename MutableBufferSequence>
		void write_with_timeout(AsyncWriteStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
			boost::optional<boost::system::error_code> timer_result;
			boost::asio::deadline_timer timer(sock.get_io_service());
			timer.expires_from_now(duration);
			timer.async_wait(boost::bind(set_result, &timer_result, _1));

			boost::optional<boost::system::error_code> read_result;
			async_write(sock, buffers, boost::bind(set_result, &read_result, _1));

			sock.get_io_service().reset();
			while (sock.get_io_service().run_one()) {
				if (read_result)
					timer.cancel();
				else if (timer_result)
					rawSocket.close();
			}

			if (*read_result)
				throw boost::system::system_error(*read_result);
		}


		struct timed_reader : boost::noncopyable {
			boost::asio::io_service &io_service;
			boost::posix_time::time_duration duration;
			boost::asio::deadline_timer timer;

			boost::optional<boost::system::error_code> timer_result;
			boost::optional<boost::system::error_code> write_result;

			timed_reader(boost::asio::io_service &io_service, boost::posix_time::time_duration duration)
				: io_service(io_service) 
				, timer(io_service)
			{
				timer.expires_from_now(duration);
				timer.async_wait(boost::bind(set_result, &timer_result, _1));
			}
			~timed_reader() {
				timer.cancel();
			}

			template <typename AsyncWriteStream, typename MutableBufferSequence>
			void read(AsyncWriteStream& socket, MutableBufferSequence &buffers) {
				async_read(socket, buffers, boost::bind(set_result, &write_result, _1));
			}

			template <typename AsyncWriteStream, typename MutableBufferSequence>
			bool read_and_wait(AsyncWriteStream& sock, MutableBufferSequence& buffers) {
				read(sock, buffers);
				return wait();
			}
			bool wait() {
				io_service.reset();
				while (io_service.run_one()) {
					if (write_result) {
						std::cout << "---read---" << std::endl;
						//timer.cancel();
						return true;
					}
					else if (timer_result) {
						std::cout << "---timer (read)---" << std::endl;
						//socket.close();
						return false;
					}
				}
			}
		};


		template <typename AsyncReadStream, typename RawSocket, typename MutableBufferSequence>
		void read_with_timeout(AsyncReadStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
			boost::optional<boost::system::error_code> timer_result;
			boost::asio::deadline_timer timer(sock.get_io_service());
			timer.expires_from_now(duration);
			timer.async_wait(boost::bind(set_result, &timer_result, _1));

			boost::optional<boost::system::error_code> read_result;
			async_read(sock, buffers, boost::bind(set_result, &read_result, _1));

			sock.get_io_service().reset();
			while (sock.get_io_service().run_one()) {
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