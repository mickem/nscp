#pragma once

#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <socket/socket_helpers.hpp>
#include <socket/connection.hpp>
#include <strEx.h>

namespace socket_helpers {

	namespace server {
		namespace str = nscp::helpers;
		
		class server_exception : public std::exception {
			std::string error;
		public:
			//////////////////////////////////////////////////////////////////////////
			/// Constructor takes an error message.
			/// @param error the error message
			///
			/// @author mickem
			//server_exception(std::wstring error) : error(to_string(error)) {}
			server_exception(std::string error) : error(error) {}
			~server_exception() throw() {}

			//////////////////////////////////////////////////////////////////////////
			/// Retrieve the error message from the exception.
			/// @return the error message
			///
			/// @author mickem
			const char* what() const throw() { return error.c_str(); }

		};

		namespace ip = boost::asio::ip;

		template<class protocol_type, std::size_t N>
		class server : private boost::noncopyable {

			typedef socket_helpers::server::connection<protocol_type, N> connection_type;
#ifdef USE_SSL
			typedef socket_helpers::server::ssl_connection<protocol_type, N> ssl_connection_type;
#endif
		public:
			server(boost::shared_ptr<protocol_type> protocol)
				: protocol_(protocol)
				, acceptor_(io_service_)
				, accept_strand_(io_service_)
#ifdef USE_SSL
				, context_(io_service_, boost::asio::ssl::context::sslv23)
#endif
			{
				if (!protocol_)
					throw server_exception("Invalid protocol instance");
			}
			~server() {

			}

			void start() {
				ip::tcp::resolver resolver(io_service_);
				ip::tcp::resolver::iterator endpoint_iterator;
				if (protocol_->get_info().address.empty()) {
					endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(protocol_->get_info().get_port()));
				} else {
					endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(protocol_->get_info().get_address(), protocol_->get_info().get_port()));
				}
				ip::tcp::resolver::iterator end;
				if (endpoint_iterator == end) {
					protocol_->log_error(__FILE__, __LINE__, "Failed to lookup: " + protocol_->get_info().get_endpoint_string());
					return;
				}
				if (protocol_->get_info().use_ssl) {
#ifdef USE_SSL
					SSL_CTX_set_cipher_list(context_.impl(), "ADH");
					protocol_->log_debug(__FILE__, __LINE__, "Using certificate: " + utf8::cvt<std::string>(protocol_->get_info().certificate));
					context_.use_tmp_dh_file(to_string(protocol_->get_info().certificate));
					context_.set_verify_mode(boost::asio::ssl::context::verify_none);
#else
					protocol_->log_error(__FILE__, __LINE__, "Not compiled with SSL");
#endif
				}
				new_connection_.reset(create_connection());

				ip::tcp::endpoint endpoint = *endpoint_iterator;
				acceptor_.open(endpoint.protocol());
				acceptor_.set_option(ip::tcp::acceptor::reuse_address(true));
				protocol_->log_debug(__FILE__, __LINE__, "Attempting to bind to: " + protocol_->get_info().get_endpoint_string());
				acceptor_.bind(endpoint);
				if (protocol_->get_info().back_log == connection_info::backlog_default)
					acceptor_.listen();
				else
					acceptor_.listen(protocol_->get_info().back_log);

				acceptor_.async_accept(new_connection_->socket(),accept_strand_.wrap(
					boost::bind(&server::handle_accept, this, boost::asio::placeholders::error)
					));
				protocol_->log_debug(__FILE__, __LINE__, "Bound to: " + protocol_->get_info().get_endpoint_string());

				for (std::size_t i = 0; i < protocol_->get_info().thread_pool_size; ++i) {
					thread_group_.create_thread(boost::bind(&boost::asio::io_service::run, &io_service_));
				}
			}

			void stop() {
				io_service_.stop();
				thread_group_.join_all();
			}

		private:
			void handle_accept(const boost::system::error_code& e) {
				if (!e) {
					std::list<std::string> errors;
					if (protocol_->on_accept(new_connection_->socket())) {
						new_connection_->start();
					} else {
						new_connection_->stop();
					}

					new_connection_.reset(create_connection());

					acceptor_.async_accept(new_connection_->socket(),
						accept_strand_.wrap(
						boost::bind(&server::handle_accept, this, boost::asio::placeholders::error)
						)
						);
				} else {
					protocol_->log_error(__FILE__, __LINE__, "Socket ERROR: " + e.message());
				}
			}

			typename connection_type* create_connection() {
#ifdef USE_SSL
				if (protocol_->get_info().use_ssl) {
					return new ssl_connection_type(io_service_, context_, protocol_);
				}
#endif
				return new connection_type(io_service_, protocol_);
			}

			boost::asio::io_service io_service_;
			boost::asio::ip::tcp::acceptor acceptor_;
			boost::shared_ptr<connection_type> new_connection_;
			boost::thread_group thread_group_;
#ifdef USE_SSL
			boost::asio::ssl::context context_;
#endif
			boost::asio::strand accept_strand_;
			boost::shared_ptr<protocol_type> protocol_;

		};

	} // namespace server
} // namespace nrpe
