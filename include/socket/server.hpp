#pragma once

#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <socket/socket_helpers.hpp>
#include <strEx.h>

namespace socket_helpers {

	namespace server {
		namespace str = nscp::helpers;
		/*
		struct dummy_handler {
		handler() {}
		handler(const handler &other) {}
		handler& operator= (const handler &other) {
		return *this;
		}
		virtual std::string handle(std::string packet) = 0;
		virtual void log_debug(std::string file, int line, std::wstring msg) = 0;
		virtual void log_error(std::string file, int line, std::wstring msg) = 0;
		virtual std::string create_error(std::wstring msg) = 0;
		};
		struct dummy_connection_info : public socket_helpers::connection_info {
		dummy_connection_info(boost::shared_ptr<dummy_handler> request_handler) : request_handler(request_handler) {}
		dummy_connection_info(const connection_info &other) 
		: socket_helpers::connection_info(other)
		, request_handler(other.request_handler)
		{}
		connection_info& operator=(const connection_info &other) {
		socket_helpers::connection_info::operator=(other);
		request_handler = other.request_handler;
		return *this;
		}
		boost::shared_ptr<dummy_handler> request_handler;
		};


		struct dummy_server_definition {
		typedef boost::shared_ptr<dummy_handler> handler_type;
		typedef dummy_connection_info connection_info_type;

		static connection_info_type create(boost::asio::io_service& io_service, boost::asio::ssl::context &context, handler_type handler, connection_info_type info);
		};

		*/
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

		template<class ServerDefinition>
		class server : boost::noncopyable {
		public:
			server(typename ServerDefinition::connection_info_type info) 
				: acceptor_(io_service_)
				, accept_strand_(io_service_)
				, request_handler_(info.request_handler)
#ifdef USE_SSL
				, context_(io_service_, boost::asio::ssl::context::sslv23)
#endif
				, info_(info) {}

			void setup() {
				if (!request_handler_)
					throw server_exception("Invalid handler when creating server");

				// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
				ip::tcp::resolver resolver(io_service_);
				ip::tcp::resolver::iterator endpoint_iterator;
				if (info_.address.empty()) {
					endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(info_.get_port()));
				} else {
					endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(info_.get_address(), info_.get_port()));
				}
				ip::tcp::resolver::iterator end;
				if (endpoint_iterator == end)
					throw server_exception("Failed to lookup: " + info_.get_endpoint_string());
#ifdef USE_SSL
				if (info_.use_ssl) {
					SSL_CTX_set_cipher_list(context_.impl(), "ADH");
					request_handler_->log_debug(__FILE__, __LINE__, _T("Using cert: ") + str::to_wstring(info_.certificate));
					context_.use_tmp_dh_file(to_string(info_.certificate));
					context_.set_verify_mode(boost::asio::ssl::context::verify_none);
				}
#endif
				new_connection_.reset(create_connection());

				ip::tcp::endpoint endpoint = *endpoint_iterator;
				acceptor_.open(endpoint.protocol());
				acceptor_.set_option(ip::tcp::acceptor::reuse_address(true));
				request_handler_->log_debug(__FILE__, __LINE__, _T("Attempting to bind to: ") + info_.get_endpoint_wstring());
				acceptor_.bind(endpoint);
				if (info_.back_log == connection_info::backlog_default)
					acceptor_.listen();
				else
					acceptor_.listen(info_.back_log);

				acceptor_.async_accept(new_connection_->socket(),
					accept_strand_.wrap(
					boost::bind(&server<ServerDefinition>::handle_accept, this, boost::asio::placeholders::error)
					)
					);
				request_handler_->log_debug(__FILE__, __LINE__, _T("Bound to: ") + info_.get_endpoint_wstring());
			}

			typename ServerDefinition::connection_type* create_connection() {
#ifdef USE_SSL
				return ServerDefinition::create(io_service_, context_, request_handler_, info_);
#else
				return ServerDefinition::create(io_service_, request_handler_, info_);
#endif
			}


			virtual ~server() {}

			/// Run the server's io_service loop.
			void start() {
				// Create a pool of threads to run all of the io_services.
				for (std::size_t i = 0; i < info_.thread_pool_size; ++i) {
					boost::shared_ptr<boost::thread> thread(
						new boost::thread( boost::bind(&boost::asio::io_service::run, &io_service_) ));
					threads_.push_back(thread);
				}
				request_handler_->log_debug(__FILE__, __LINE__, _T("Thredpool containes: ") + str::to_wstring(info_.thread_pool_size));
			}

			void stop() {
				io_service_.stop();
				for (std::size_t i = 0; i < threads_.size(); ++i)
					threads_[i]->join();
			}

			void handle_accept(const boost::system::error_code& e) {
				if (!e) {
					std::list<std::string> errors;
					try {
						if (info_.allowed_hosts.is_allowed(new_connection_->socket().remote_endpoint().address(), errors)) {
							std::string s = new_connection_->socket().remote_endpoint().address().to_string();
							request_handler_->log_debug(__FILE__, __LINE__, _T("Accepting connection from: ") + str::to_wstring(s));
							new_connection_->start();
						} else {
							BOOST_FOREACH(const std::string &e, errors) {
								request_handler_->log_error(__FILE__, __LINE__, utf8::cvt<std::wstring>(e));
							}
							std::string s = new_connection_->socket().remote_endpoint().address().to_string();
							request_handler_->log_error(__FILE__, __LINE__, _T("Rejcted connection from: ") + str::to_wstring(s));
							new_connection_->stop();
						}
					} catch(const std::exception &e) {
						new_connection_->stop();
						request_handler_->log_error(__FILE__, __LINE__, utf8::cvt<std::wstring>(e.what()));
					}
					new_connection_.reset(create_connection());
					acceptor_.async_accept(new_connection_->socket(),
						accept_strand_.wrap(boost::bind(&server<ServerDefinition>::handle_accept, this, boost::asio::placeholders::error))
					);
				} else {
					request_handler_->log_error(__FILE__, __LINE__, _T("Socket ERROR: ") + str::to_wstring(e.message()));
				}
			}


		private:

			boost::asio::io_service io_service_;

			boost::asio::ip::tcp::acceptor acceptor_;

			boost::shared_ptr<typename ServerDefinition::connection_type> new_connection_;

			std::vector<boost::shared_ptr<boost::thread> > threads_;

			typename ServerDefinition::handler_type request_handler_;

#ifdef USE_SSL
			boost::asio::ssl::context context_;
#endif

			boost::asio::strand accept_strand_;

			typename ServerDefinition::connection_info_type info_;
		};

	} // namespace server
} // namespace nrpe
