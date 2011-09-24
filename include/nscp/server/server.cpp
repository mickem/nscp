#include "server.hpp"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>


namespace nscp {
	namespace server {



		namespace ip = boost::asio::ip;


		server::server(connection_info info)
			: acceptor_(io_service_)
			, accept_strand_(io_service_)
			, request_handler_(info.request_handler) // nscp::length::get_payload_length())
			, context_(io_service_, boost::asio::ssl::context::sslv23)
			, info_(info)
		{
			if (!request_handler_)
				throw nscp_exception(_T("Invalid handler"));
			// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
			ip::tcp::resolver resolver(io_service_);
			ip::tcp::resolver::iterator endpoint_iterator;
			if (info_.address.empty()) {
				endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(info_.get_port()));
			} else {
				endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(info_.get_address(), info_.get_port()));
			}
			ip::tcp::resolver::iterator end;
			if (endpoint_iterator == end) {
				request_handler_->log_error(__FILE__, __LINE__, std::wstring(_T("Failed to lookup: ")) + info_.get_endpoint_wstring());
				return;
			}
			if (info_.use_ssl) {
				SSL_CTX_set_cipher_list(context_.impl(), "ADH");
				request_handler_->log_debug(__FILE__, __LINE__, _T("Using cert: ") + to_wstring(info_.certificate));
				context_.use_tmp_dh_file(to_string(info_.certificate));
				context_.set_verify_mode(boost::asio::ssl::context::verify_none);
			}

			new_connection_.reset(nscp::server::factories::create(io_service_, context_, request_handler_, info_.use_ssl));

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
					boost::bind(&server::handle_accept, this, boost::asio::placeholders::error)
					)
				);
			request_handler_->log_debug(__FILE__, __LINE__, _T("Bound to: ") + info_.get_endpoint_wstring());

			//io_service_.post(boost::bind(&Server::startAccept, this));

		}

		server::~server() {}

		void server::start() {
			// Create a pool of threads to run all of the io_services.
			for (std::size_t i = 0; i < info_.thread_pool_size; ++i) {
				boost::shared_ptr<boost::thread> thread(
					new boost::thread( boost::bind(&boost::asio::io_service::run, &io_service_) ));
				threads_.push_back(thread);
			}
			request_handler_->log_debug(__FILE__, __LINE__, _T("Thredpool containes: ") + to_wstring(info_.thread_pool_size));

			// Wait for all threads in the pool to exit.
			//for (std::size_t i = 0; i < threads.size(); ++i)
			//	threads[i]->join();
		}

		void server::stop() {
			io_service_.stop();
			for (std::size_t i = 0; i < threads_.size(); ++i)
				threads_[i]->join();
		}

		void server::handle_accept(const boost::system::error_code& e) {
			if (!e) {
				std::list<std::string> errors;
				boost::asio::ip::address a = new_connection_->socket().remote_endpoint().address();
				std::stringstream ss;
				ss << "address: " << a.to_string() << " - ";
				ss << "v4: " << a.is_v4() << " - ";
				ss << "v6: " << a.is_v6() << " - ";
				request_handler_->log_error(__FILE__, __LINE__, _T(" -- ") + to_wstring(ss.str()));
				if (a.is_v6()) {
					ss << "v4: " << a.to_v6().is_v4_compatible() << " - ";
					ss << "v4: " << a.to_v6().is_v4_mapped() << " - ";
					try {
						ss << "v4: " << a.to_v6().to_v4().to_string() << " - ";
					} catch (...) {
						ss << "failed" << " - ";
					}
				}
				request_handler_->log_error(__FILE__, __LINE__, _T(" -- ?? --"));
				request_handler_->log_error(__FILE__, __LINE__, _T(" -- ") + to_wstring(ss.str()));
				request_handler_->log_error(__FILE__, __LINE__, _T(" -- ?? --"));

				std::string s = a.to_string();
				if (info_.allowed_hosts.is_allowed(a, errors)) {
					request_handler_->log_debug(__FILE__, __LINE__, _T("Accepting connection from: ") + to_wstring(s));
					new_connection_->start();
				} else {
					BOOST_FOREACH(const std::string &e, errors) {
						request_handler_->log_error(__FILE__, __LINE__, utf8::cvt<std::wstring>(e));
					}
					request_handler_->log_error(__FILE__, __LINE__, _T("Rejcted connection from: ") + to_wstring(s));
					new_connection_->stop();
				}

				new_connection_.reset(nscp::server::factories::create(io_service_, context_, request_handler_, info_.use_ssl));

				acceptor_.async_accept(new_connection_->socket(),
					accept_strand_.wrap(
						boost::bind(&server::handle_accept, this, boost::asio::placeholders::error)
						)
					);
			} else {
				request_handler_->log_error(__FILE__, __LINE__, _T("Socket ERROR: ") + to_wstring(e.message()));
			}
		}

		void start() {
		}

	} // namespace server
} // namespace nscp
