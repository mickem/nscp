#pragma once

#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <iostream>

using boost::asio::ip::tcp;

namespace socket_helpers {
	namespace client {

		static const bool debug_trace = true;

		template<class protocol_type>
		class connection : public boost::enable_shared_from_this<connection<protocol_type> >, private boost::noncopyable {
		private:
			tcp::socket socket_;
			protocol_type protocol_;
			boost::asio::io_service &io_service_;
			boost::asio::deadline_timer timer_;
			boost::posix_time::time_duration timeout_;
			boost::optional<boost::system::error_code> timer_result_;
			boost::optional<bool> data_result_;
			boost::shared_ptr<typename protocol_type::client_handler> handler_;

		public:
			connection(boost::asio::io_service &io_service, boost::posix_time::time_duration timeout, boost::shared_ptr<typename protocol_type::client_handler> handler) 
				: io_service_(io_service)
				, socket_(io_service) 
				, timer_(io_service)
				, timeout_(timeout)
				, handler_(handler)
				, protocol_(handler)
			{}

			virtual ~connection() {
				stop_timer();
				close();
			}

			typedef boost::asio::basic_socket<tcp,boost::asio::stream_socket_service<tcp> >  basic_socket_type;

			//////////////////////////////////////////////////////////////////////////
			// Time related functions
			//
			void start_timer() {
				timer_result_.reset();
				timer_.expires_from_now(timeout_);
				timer_.async_wait(boost::bind(&connection::on_timeout, shared_from_this(),  boost::asio::placeholders::error));
			}
			void stop_timer() {
				timer_.cancel();
			}
			virtual void on_timeout(boost::system::error_code ec) {
				trace("on_timeout(" + ec.message() + ")");
				if (!ec) {
					timer_result_.reset(ec);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// External API functions
			//
			virtual void connect(std::string host, std::string port) {
				tcp::resolver resolver(io_service_);
				tcp::resolver::query query(host, port);

				tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
				tcp::resolver::iterator end;

				boost::system::error_code error = boost::asio::error::host_not_found;
				while (error && endpoint_iterator != end) {
					tcp::resolver::endpoint_type ep = *endpoint_iterator;
					get_socket().close();
					get_socket().lowest_layer().connect(*endpoint_iterator++, error);
				}
				if (error)
					throw boost::system::system_error(error);
				protocol_.on_connect();
			}

			virtual typename protocol_type::response_type process_request(typename protocol_type::request_type &packet) {
				start_timer();
				data_result_.reset();
				protocol_.prepare_request(packet);
				do_process();
				if (!wait()) {
					stop_timer();
					close();
					return protocol_.get_timeout_response();
				}
				stop_timer();
				return protocol_.get_response();
			}

			virtual void shutdown() {
				trace("shutdown()");
				boost::system::error_code ignored_ec;
				if (get_socket().is_open())
					get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			};


			virtual void close() {
				trace("close()");
				if (!get_socket().is_open())
					return;
				get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
				get_socket().close();
			}

			//////////////////////////////////////////////////////////////////////////
			// Internal socket functions
			//
			virtual void do_process() {
				trace("do_process()");
				if (protocol_.wants_data()) {
					start_read_request(boost::asio::buffer(protocol_.get_inbound()));
				} else if (protocol_.has_data()) {
					start_write_request(boost::asio::buffer(protocol_.get_outbound()));
				} else {
					trace("do_process(done)");
					data_result_.reset(true);
				}
			}

			virtual void start_read_request(boost::asio::mutable_buffers_1 &buffer) {
				trace("start_read_request()");
				async_read(socket_, buffer, 
					boost::bind(&connection::handle_read_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
				);
			}

			virtual void handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
				trace("handle_read_request(" + strEx::s::itos((int)bytes_transferred) + ")");
				if (!e) {
					protocol_.on_read(bytes_transferred);
					do_process();
				} else {
					handler_->log_error(__FILE__, __LINE__, "Failed to read data: " + e.message());
				}
			}

			virtual void start_write_request(boost::asio::mutable_buffers_1 &buffer) {
				trace("start_write_request()");
				async_write(socket_, buffer, 
					boost::bind(&connection::handle_write_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					);
			}

			virtual void handle_write_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
				trace("handle_write_request(" + strEx::s::itos((int)bytes_transferred) + ")");
				if (!e) {
					protocol_.on_write(bytes_transferred);
					do_process();
				} else {
					handler_->log_error(__FILE__, __LINE__, "Failed to send data: " + e.message());
				}
			}

			virtual bool wait() {
				trace("wait()");
				io_service_.reset();
				while (io_service_.run_one()) {
					if (data_result_) {
						trace("data_result()");
						return true;
					}
					else if (timer_result_) {
						trace("timer_result()");
						return false;
					}
				}
				return false;
			}
			virtual basic_socket_type& get_socket() {
				return socket_;
			}

			//////////////////////////////////////////////////////////////////////////
			// Internal helper functions
			//
			inline void trace(std::string msg) const {
				if (debug_trace) 
					handler_->log_debug(__FILE__, __LINE__, msg);
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
				, ssl_socket_(io_service, context)
			{}
			virtual ~ssl_connection() {
			}


			virtual void connect(std::string host, std::string port) {
				connection_type::connect(host, port);
				ssl_socket_.handshake(boost::asio::ssl::stream_base::client);
			}

			virtual void start_read_request(boost::asio::mutable_buffers_1 &buffer) {
				trace("ssl::start_read_request()");
				async_read(ssl_socket_, buffer, 
					boost::bind(&connection::handle_read_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					);
			}

			virtual void start_write_request(boost::asio::mutable_buffers_1 &buffer) {
				trace("ssl::start_write_request()");
				async_write(ssl_socket_, buffer, 
					boost::bind(&connection::handle_write_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					);
			}
			virtual basic_socket_type& get_socket() {
				return ssl_socket_.lowest_layer();
			}
		};
#endif

		template<class protocol_type>
		class client {
			boost::shared_ptr<connection<protocol_type> > connection_;
			boost::asio::io_service io_service_;
			boost::shared_ptr<typename protocol_type::client_handler> handler_;

			typedef connection<protocol_type> connection_type;
#ifdef USE_SSL
			boost::asio::ssl::context context_;
			typedef ssl_connection<protocol_type> ssl_connection_type;
#endif

		public:
			client(typename boost::shared_ptr<typename protocol_type::client_handler> handler)
				: handler_(handler)
#ifdef USE_SSL
				, context_(io_service_, boost::asio::ssl::context::sslv23)
#endif
			{
			}

			void connect() {
				connection_.reset(create_connection());
				connection_->connect(handler_->get_host(), handler_->get_port());
			}

			typename connection_type* create_connection() {
#ifdef USE_SSL
				if (handler_->use_ssl()) {
					connection_type* ptr = new ssl_connection_type(io_service_, context_, handler_->get_timeout(), handler_);
					handler_->setup_ssl(context_);
					return ptr;
				}
#endif
				return new connection_type(io_service_, handler_->get_timeout(), handler_);
			}

			typename protocol_type::response_type process_request(typename protocol_type::request_type &packet) {
				return connection_->process_request(packet);
			}
			void shutdown() {
				connection_->shutdown();
			};

		};

		struct client_handler : private boost::noncopyable {

			std::string dh_key_;
			std::string host_;
			std::string port_;
			long timeout_;
			bool ssl_;

			client_handler(std::string host, std::string port, long timeout, bool ssl, std::string dh_key)
				: host_(host)
				, port_(port)
				, timeout_(timeout)
				, ssl_(ssl)
				, dh_key_(dh_key)
			{}

			bool use_ssl() { return ssl_; }
			std::string get_host() { return host_; }
			std::string get_port() { return port_; }
			boost::posix_time::time_duration get_timeout() { return boost::posix_time::seconds(timeout_); }
#ifdef USE_SSL
			void setup_ssl(boost::asio::ssl::context &context) {
				SSL_CTX_set_cipher_list(context.impl(), "ADH");
				context.use_tmp_dh_file(dh_key_);
				context.set_verify_mode(boost::asio::ssl::context::verify_none);
			}
#endif

			virtual void log_debug(std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string file, int line, std::string msg) const = 0;

		};
	
	}
}