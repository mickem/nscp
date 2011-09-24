#pragma once

#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

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
		bool is_allowed(const boost::asio::ip::address &address, std::list<std::string> &errors) {
			return (address.is_v4() && is_allowed_v4(address.to_v4().to_ulong(), errors))
				|| (address.is_v6() && address.to_v6().is_v4_compatible() && is_allowed_v4(address.to_v6().to_v4().to_ulong(), errors))
				|| (address.is_v6() && address.to_v6().is_v4_mapped() && is_allowed_v4(address.to_v6().to_v4().to_ulong(), errors));
		}
		bool is_allowed_v4(const unsigned long &remote, std::list<std::string> &errors) {
			errors.push_back(strEx::wstring_to_string(strEx::itos(remote)));
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
		std::string get_endpoint_string() {
			return address + ":" + get_port();
		}
		std::wstring get_endpoint_wstring() {
			return utf8::cvt<std::wstring>(get_endpoint_string());
		}
	};



	namespace io {
		void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b);

		struct timed_writer : public boost::enable_shared_from_this<timed_writer> {
			boost::asio::io_service &io_service;
			boost::posix_time::time_duration duration;
			boost::asio::deadline_timer timer;

			boost::optional<boost::system::error_code> timer_result;
			boost::optional<boost::system::error_code> read_result;

			timed_writer(boost::asio::io_service& io_service) : io_service(io_service), timer(io_service) {}
			~timed_writer() {
				timer.cancel();
			}
			void start_timer(boost::posix_time::time_duration duration) {
				timer.expires_from_now(duration);
				timer.async_wait(boost::bind(&timed_writer::set_result, shared_from_this(), &timer_result, _1));
			}
			void stop_timer() {
				timer.cancel();
			}

			template <typename AsyncWriteStream, typename MutableBufferSequence>
			void write(AsyncWriteStream& stream, MutableBufferSequence &buffer) {
				async_write(stream, buffer, boost::bind(&timed_writer::set_result, shared_from_this(), &read_result, _1));
			}

			template <typename AsyncWriteStream, typename Socket, typename MutableBufferSequence>
			bool write_and_wait(AsyncWriteStream& stream, Socket& socket, const MutableBufferSequence& buffer) {
				write(stream, buffer);
				return wait(socket);
			}

			template<typename Socket>
			bool wait(Socket& socket) {
				io_service.reset();
				while (io_service.run_one()) {
					if (read_result) {
						read_result.reset();
						return true;
					}
					else if (timer_result) {
						socket.close();
						return false;
					}
				}
			}

			void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code ec) {
				if (!ec)
					a->reset(ec);
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


		struct timed_reader : public boost::enable_shared_from_this<timed_reader> {
			boost::asio::io_service &io_service;
			boost::posix_time::time_duration duration;
			boost::asio::deadline_timer timer;

			boost::optional<boost::system::error_code> timer_result;
			boost::optional<boost::system::error_code> write_result;

			timed_reader(boost::asio::io_service &io_service) : io_service(io_service), timer(io_service) {}
			~timed_reader() {
				timer.cancel();
			}

			void start_timer(boost::posix_time::time_duration duration) {
				timer.expires_from_now(duration);
				timer.async_wait(boost::bind(&timed_reader::set_result, shared_from_this(), &timer_result, _1));
			}
			void stop_timer() {
				timer.cancel();
			}

			template <typename AsyncWriteStream, typename MutableBufferSequence>
			void read(AsyncWriteStream& stream, const MutableBufferSequence &buffers) {
				async_read(stream, buffers, boost::bind(&timed_reader::set_result, shared_from_this(), &write_result, _1));
			}

			template <typename AsyncWriteStream, typename Socket, typename MutableBufferSequence>
			bool read_and_wait(AsyncWriteStream& stream, Socket& socket, const MutableBufferSequence& buffers) {
				read(stream, buffers);
				return wait(socket);
			}
			template <typename Socket>
			bool wait(Socket& socket) {
				io_service.reset();
				while (io_service.run_one()) {
					if (write_result) {
						write_result.reset();
						return true;
					}
					else if (timer_result) {
						socket.close();
						return false;
					}
				}
			}
			void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code ec) {
				if (!ec)
					a->reset(ec);
			}

		};


		template <typename AsyncReadStream, typename RawSocket, typename MutableBufferSequence>
		bool read_with_timeout(AsyncReadStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
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
				else if (timer_result) {
					rawSocket.close();
					return false;
				} else {
					if (!rawSocket.is_open()) {
						timer.cancel();
						rawSocket.close();
						return false;
					}
				}
			}

			if (*read_result)
				throw boost::system::system_error(*read_result);
			return true;
		}
	}
}