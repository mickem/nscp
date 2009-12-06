#pragma once

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include "nrpe_connection.hpp"
//#include "request_handler.hpp"

namespace nrpe {
	namespace server {
		class server : private boost::noncopyable {
		public:
			/// Construct the server to listen on the specified TCP address and port, and
			/// serve up files from the given directory.
			explicit server(const std::string& address, const std::string& port, const std::string& doc_root, std::size_t thread_pool_size);

			virtual ~server();

			/// Run the server's io_service loop.
			void start();

			/// Stop the server.
			void stop();

		private:
			/// Handle completion of an asynchronous accept operation.
			void handle_accept(const boost::system::error_code& e);

			/// The number of threads that will call io_service::run().
			std::size_t thread_pool_size_;

			/// The io_service used to perform asynchronous operations.
			boost::asio::io_service io_service_;

			/// Acceptor used to listen for incoming connections.
			boost::asio::ip::tcp::acceptor acceptor_;

			/// The next connection to be accepted.
			connection_ptr new_connection_;

			std::vector<boost::shared_ptr<boost::thread> > threads_;

			/// The handler for all incoming requests.
			nrpe::server::handler request_handler_;
		};

	} // namespace server
} // namespace nrpe
