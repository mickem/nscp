#include "stdafx.h"

#include "nrpe_server.hpp"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace nrpe {
	namespace server {
		server::server(const std::string& address, const std::string& port,
			const std::string& doc_root, std::size_t thread_pool_size)
			: thread_pool_size_(thread_pool_size)
			, acceptor_(io_service_)
			, request_handler_(nrpe::length::get_payload_length())
			, new_connection_(new connection(io_service_, request_handler_))
			/*,request_handler_(doc_root)*/
		{
			// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
			boost::asio::ip::tcp::resolver resolver(io_service_);
			boost::asio::ip::tcp::resolver::query query(address, port);
			boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
			boost::asio::ip::tcp::resolver::iterator end;
			if (endpoint_iterator == end) {
				std::cout << "Failed to lookup: " << address << ":" << port << std::endl;
			}
			boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
			std::cout << "Binding to: " << address << ":" << port << std::endl;
			acceptor_.open(endpoint.protocol());
			acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
			acceptor_.bind(endpoint);
			acceptor_.listen();
			acceptor_.async_accept(new_connection_->socket(),
				boost::bind(&server::handle_accept, this, boost::asio::placeholders::error));
			std::cout << "Bound to: " << address << ":" << port << std::endl;
		}

		server::~server() {
			std::cout << "Destroying server... "<< std::endl;
		}

		void server::start() {
			// Create a pool of threads to run all of the io_services.
			for (std::size_t i = 0; i < thread_pool_size_; ++i) {
				boost::shared_ptr<boost::thread> thread(
					new boost::thread( boost::bind(&boost::asio::io_service::run, &io_service_) ));
				threads_.push_back(thread);
			}
			std::cout << "Threads started: " << thread_pool_size_ << std::endl;

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
			std::cout << "accept: " << e.message() << std::endl;
			if (!e) {
				new_connection_->start();
				new_connection_.reset(new connection(io_service_, request_handler_));
				acceptor_.async_accept(new_connection_->socket(),
					boost::bind(&server::handle_accept, this,
					boost::asio::placeholders::error));
			} else {
				std::cout << "ERROR: " << e.message() << std::endl;
			}
		}
	} // namespace server
} // namespace nrpe
