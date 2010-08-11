#include "server.hpp"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>


namespace nrpe {
	namespace server {



		namespace ip = boost::asio::ip;

		server::server(connection_info info)
			: thread_pool_size_(info.thread_pool_size)
			, acceptor_(io_service_)
			, accept_strand_(io_service_)
			, request_handler_(info.request_handler) // nrpe::length::get_payload_length())
			, context_(io_service_, boost::asio::ssl::context::sslv23)
			, use_ssl_(info.use_ssl)
		{
			if (!request_handler_)
				throw nrpe_exception(_T("Invalid handler"));
			// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
			ip::tcp::resolver resolver(io_service_);
			ip::tcp::resolver::iterator endpoint_iterator;
			if (info.address.empty()) {
				endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(info.port));
			} else {
				endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(info.address, info.port));
			}
			ip::tcp::resolver::iterator end;
			if (endpoint_iterator == end) {
				request_handler_->log_error(__FILEW__, __LINE__, std::wstring(_T("Failed to lookup: ")) + info.get_endpoint_str());
				return;
			}
			if (info.use_ssl) {
				SSL_CTX_set_cipher_list(context_.impl(), "ADH");
				request_handler_->log_debug(__FILEW__, __LINE__, _T("Using cert: ") + to_wstring(info.certificate));
				context_.use_tmp_dh_file(info.certificate);
				context_.set_verify_mode(boost::asio::ssl::context::verify_none);
			}

			new_connection_.reset(nrpe::server::factories::create(io_service_, context_, request_handler_, use_ssl_));

			ip::tcp::endpoint endpoint = *endpoint_iterator;
			acceptor_.open(endpoint.protocol());
			acceptor_.set_option(ip::tcp::acceptor::reuse_address(true));
			acceptor_.bind(endpoint);
			acceptor_.listen();

			acceptor_.async_accept(new_connection_->socket(),
				accept_strand_.wrap(
					boost::bind(&server::handle_accept, this, boost::asio::placeholders::error)
					)
				);
			request_handler_->log_debug(__FILEW__, __LINE__, _T("Bound to: ") + info.get_endpoint_str());

			//io_service_.post(boost::bind(&Server::startAccept, this));

		}

		server::~server() {}

		void server::start() {
			// Create a pool of threads to run all of the io_services.
			for (std::size_t i = 0; i < thread_pool_size_; ++i) {
				boost::shared_ptr<boost::thread> thread(
					new boost::thread( boost::bind(&boost::asio::io_service::run, &io_service_) ));
				threads_.push_back(thread);
			}
			request_handler_->log_debug(__FILEW__, __LINE__, _T("Thredpool containes: ") + to_wstring(thread_pool_size_));

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
				std::string s = new_connection_->socket().remote_endpoint().address().to_string();
				request_handler_->log_debug(__FILEW__, __LINE__, _T("Accepting connection from: ") + to_wstring(s));

				new_connection_->start();
				new_connection_.reset(nrpe::server::factories::create(io_service_, context_, request_handler_, use_ssl_));

				acceptor_.async_accept(new_connection_->socket(),
					accept_strand_.wrap(
						boost::bind(&server::handle_accept, this, boost::asio::placeholders::error)
						)
					);
			} else {
				request_handler_->log_error(__FILEW__, __LINE__, _T("Socket ERROR: ") + to_wstring(e.message()));
			}
		}

		void start() {
		}

	} // namespace server
} // namespace nrpe
