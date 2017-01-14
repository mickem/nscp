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

#pragma once

#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <iostream>

using boost::asio::ip::tcp;

namespace socket_helpers {
	namespace client {
		template<class protocol_type>
		class connection : public boost::enable_shared_from_this<connection<protocol_type> >, private boost::noncopyable {
		private:
			boost::asio::io_service &io_service_;
			boost::asio::deadline_timer timer_;
			boost::posix_time::time_duration timeout_;
			boost::shared_ptr<typename protocol_type::client_handler> handler_;
			protocol_type protocol_;

			boost::optional<boost::system::error_code> timer_result_;
			boost::optional<bool> data_result_;

		public:
			connection(boost::asio::io_service &io_service, boost::posix_time::time_duration timeout, boost::shared_ptr<typename protocol_type::client_handler> handler)
				: io_service_(io_service)
				, timer_(io_service)
				, timeout_(timeout)
				, handler_(handler)
				, protocol_(handler) {}

			virtual ~connection() {
				try {
					cancel_timer();
				} catch (const std::exception &e) {
					handler_->log_error(__FILE__, __LINE__, std::string("Failed to close connection: ") + utf8::utf8_from_native(e.what()));
				} catch (...) {
					handler_->log_error(__FILE__, __LINE__, "Failed to close connection");
				}
			}

			typedef boost::asio::basic_socket<tcp, boost::asio::stream_socket_service<tcp> >  basic_socket_type;

			//////////////////////////////////////////////////////////////////////////
			// Time related functions
			//
			void start_timer() {
				timer_result_.reset();
				timer_.expires_from_now(timeout_);
				timer_.async_wait(boost::bind(&connection::on_timeout, this->shared_from_this(), boost::asio::placeholders::error));
			}
			void cancel_timer() {
				trace("cancel_timer()");
				timer_.cancel();
			}
			virtual void on_timeout(boost::system::error_code ec) {
				trace("on_timeout(" + utf8::utf8_from_native(ec.message()) + ")");
				if (!ec) {
					timer_result_.reset(ec);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// External API functions
			//
			virtual boost::system::error_code connect(std::string host, std::string port) {
				trace("connect(" + host + ", " + port + ")");
				tcp::resolver resolver(io_service_);
				tcp::resolver::query query(host, port, boost::asio::ip::resolver_query_base::numeric_service);

				tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
				tcp::resolver::iterator end;

				boost::system::error_code error = boost::asio::error::host_not_found;
				while (error && endpoint_iterator != end) {
					get_socket().close();
					get_socket().lowest_layer().connect(*endpoint_iterator++, error);
				}
				if (error) {
					trace("Failed to connect to: " + host + ":" + port);
					return error;
				}
				protocol_.on_connect();
				return error;
			}

			virtual boost::optional<typename protocol_type::response_type> process_request(typename protocol_type::request_type &packet) {
				start_timer();
				data_result_.reset();
				protocol_.prepare_request(packet);
				do_process();
				if (!wait()) {
					close_socket();
					timer_result_.reset();
					wait();
					cancel_timer();
					return boost::optional<typename protocol_type::response_type>();
				}
				cancel_timer();
				return boost::optional<typename protocol_type::response_type>(protocol_.get_response());
			}

			virtual void shutdown() {
				trace("shutdown()");
				cancel_timer();
				close_socket();
			};

			virtual void close_socket() {
				trace("close_socket()");
				boost::system::error_code ignored_ec;
				if (get_socket().is_open()) {
					get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
					get_socket().close(ignored_ec);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Internal socket functions
			//
			void do_process() {
				trace("do_process()");
				if (protocol_.wants_data()) {
					this->start_read_request(boost::asio::buffer(protocol_.get_inbound()));
				} else if (protocol_.has_data()) {
					this->start_write_request(boost::asio::buffer(protocol_.get_outbound()));
				} else {
					trace("do_process(done)");
					data_result_.reset(true);
				}
			}

			virtual void start_read_request(boost::asio::mutable_buffers_1 buffer) = 0;

			virtual void handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
				trace("handle_read_request(" + utf8::utf8_from_native(e.message()) + ", " + str::xtos(bytes_transferred) + ")");
				if (!e) {
					protocol_.on_read(bytes_transferred);
					do_process();
				} else {
					if (bytes_transferred > 0) {
						protocol_.on_read(bytes_transferred);
					}
					if (!protocol_.on_read_error(e)) {
						handler_->log_error(__FILE__, __LINE__, "Failed to read data: " + utf8::utf8_from_native(e.message()));
						cancel_timer();
					} else {
						do_process();
					}
				}
			}

			virtual void start_write_request(boost::asio::mutable_buffers_1 buffer) = 0;

			virtual void handle_write_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
				trace("handle_write_request(" + utf8::utf8_from_native(e.message()) + ", " + str::xtos(bytes_transferred) + ")");
				if (!e) {
					protocol_.on_write(bytes_transferred);
					do_process();
				} else {
					handler_->log_error(__FILE__, __LINE__, "Failed to send data: " + utf8::utf8_from_native(e.message()));
					cancel_timer();
				}
			}

			virtual bool wait() {
				trace("wait()");
				io_service_.reset();
				while (io_service_.run_one()) {
					if (data_result_) {
						trace("data_result()");
						return true;
					} else if (timer_result_) {
						trace("timer_result()");
						return false;
					}
				}
				return false;
			}
			//////////////////////////////////////////////////////////////////////////
			// Internal helper functions
			//
			inline void trace(std::string msg) const {
				if (protocol_type::debug_trace && handler_)
					handler_->log_debug(__FILE__, __LINE__, msg);
			}
			inline void log_error(std::string file, int line, std::string msg) const {
				if (handler_)
					handler_->log_error(__FILE__, __LINE__, msg);
			}

			virtual basic_socket_type& get_socket() = 0;
		};

		template<class protocol_type>
		class tcp_connection : public connection<protocol_type> {
			typedef connection<protocol_type> connection_type;
			tcp::socket socket_;

		public:
			tcp_connection(boost::asio::io_service &io_service, boost::posix_time::time_duration timeout, boost::shared_ptr<typename protocol_type::client_handler> handler)
				: connection_type(io_service, timeout, handler)
				, socket_(io_service) {}
			virtual ~tcp_connection() {
				try {
					this->close_socket();
				} catch (const std::exception &e) {
					this->log_error(__FILE__, __LINE__, std::string("Failed to close connection: ") + utf8::utf8_from_native(e.what()));
				} catch (...) {
					this->log_error(__FILE__, __LINE__, "Failed to close connection");
				}
			}

			virtual void start_read_request(boost::asio::mutable_buffers_1 buffer) {
				this->trace("tcp::start_read_request(" + str::xtos(boost::asio::buffer_size(buffer)) + ")");
				async_read(socket_, buffer,
					boost::bind(&connection_type::handle_read_request, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					);
			}

			virtual void start_write_request(boost::asio::mutable_buffers_1 buffer) {
				this->trace("tcp::start_write_request(" + str::xtos(boost::asio::buffer_size(buffer)) + ")");
				async_write(socket_, buffer,
					boost::bind(&connection_type::handle_write_request, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					);
			}

			virtual typename connection_type::basic_socket_type& get_socket() {
				return socket_;
			}
		};

#ifdef USE_SSL
		template<class protocol_type>
		class ssl_connection : public connection<protocol_type> {
		private:
			typedef connection<protocol_type> connection_type;
			boost::asio::ssl::stream<tcp::socket> ssl_socket_;

		public:
			ssl_connection(boost::asio::io_service &io_service, boost::asio::ssl::context &context, boost::posix_time::time_duration timeout, boost::shared_ptr<typename protocol_type::client_handler> handler)
				: connection_type(io_service, timeout, handler)
				, ssl_socket_(io_service, context) {}
			virtual ~ssl_connection() {
				try {
					this->close_socket();
				} catch (const std::exception &e) {
					this->log_error(__FILE__, __LINE__, std::string("Failed to close connection: ") + utf8::utf8_from_native(e.what()));
				} catch (...) {
					this->log_error(__FILE__, __LINE__, "Failed to close connection");
				}
			}

			virtual boost::system::error_code connect(std::string host, std::string port) {
				boost::system::error_code error = connection_type::connect(host, port);
				if (error) {
					this->log_error(__FILE__, __LINE__, "Failed to connect to server: " + utf8::utf8_from_native(error.message()));
				}
				if (!error) {
					ssl_socket_.handshake(boost::asio::ssl::stream_base::client, error);
					if (error) {
						this->log_error(__FILE__, __LINE__, "SSL handshake failed: " + utf8::utf8_from_native(error.message()));
					}
				}
				return error;
			}

			virtual void start_read_request(boost::asio::mutable_buffers_1 buffer) {
				this->trace("ssl::start_read_request()");
				async_read(ssl_socket_, buffer,
					boost::bind(&connection_type::handle_read_request, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					);
			}

			virtual void start_write_request(boost::asio::mutable_buffers_1 buffer) {
				this->trace("ssl::start_write_request()");
				async_write(ssl_socket_, buffer,
					boost::bind(&connection_type::handle_write_request, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					);
			}
			virtual typename connection_type::basic_socket_type& get_socket() {
				return ssl_socket_.lowest_layer();
			}
		};
#endif

		template<class protocol_type>
		class client : boost::noncopyable {
			boost::shared_ptr<connection<protocol_type> > connection_;
			boost::asio::io_service io_service_;
			const socket_helpers::connection_info &info_;
			boost::shared_ptr<typename protocol_type::client_handler> handler_;

			typedef connection<protocol_type> connection_type;
			typedef tcp_connection<protocol_type> tcp_connection_type;
#ifdef USE_SSL
			boost::asio::ssl::context context_;
			typedef ssl_connection<protocol_type> ssl_connection_type;
#endif

		public:
			client(const socket_helpers::connection_info &info, typename boost::shared_ptr<typename protocol_type::client_handler> handler)
				: info_(info), handler_(handler)
#ifdef USE_SSL
				, context_(io_service_, boost::asio::ssl::context::sslv23)
#endif
			{}
			~client() {
				try {
					if (connection_)
						connection_->shutdown();
				} catch (...) {
					handler_->log_error(__FILE__, __LINE__, "Failed to close socket on disconnect");
				}
				connection_.reset();
			}

			void connect() {
				connection_.reset(create_connection());
				boost::system::error_code error = connection_->connect(info_.get_address(), info_.get_port());
				if (error) {
					connection_.reset();
					throw socket_helpers::socket_exception("Failed to connect to: " + info_.get_endpoint_string() + " :" + utf8::utf8_from_native(error.message()));
				}
			}

			connection_type* create_connection() {
				boost::posix_time::time_duration timeout(boost::posix_time::seconds(info_.timeout));

#ifdef USE_SSL
				if (info_.ssl.enabled) {
					std::list<std::string> errors;
					info_.ssl.configure_ssl_context(context_, errors);
					BOOST_FOREACH(const std::string &e, errors) {
						handler_->log_error(__FILE__, __LINE__, e);
					}
					return new ssl_connection_type(io_service_, context_, timeout, handler_);
				}
#endif
				return new tcp_connection_type(io_service_, timeout, handler_);
			}

			typename protocol_type::response_type process_request(typename protocol_type::request_type &packet) {
				if (!connection_)
					connect();
				boost::optional<typename protocol_type::response_type> response = connection_->process_request(packet);
				if (!response) {
					for (int i = 0; i < info_.retry; i++) {
						handler_->log_debug(__FILE__, __LINE__, "Retrying attempt " + str::xtos(i) + " of " + str::xtos(info_.retry));
						connect();
						response = connection_->process_request(packet);
						if (response)
							return *response;
					}
					handler_->log_debug(__FILE__, __LINE__, "Retrying failed");
					throw socket_helpers::socket_exception("Retry failed");
				}
				return *response;
			}
			void shutdown() {
				connection_->shutdown();
				connection_.reset();
			};
		};

		struct client_handler : private boost::noncopyable {
			client_handler() {}
			virtual ~client_handler() {}

			virtual void log_debug(std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string file, int line, std::string msg) const = 0;
			virtual std::string expand_path(std::string path) = 0;
		};
	}
}