#pragma once

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <nrpe/server/connection.hpp>
#include "handler.hpp"

namespace nrpe {

	namespace server {

		class nrpe_exception {
			std::wstring error_;
		public:
			//////////////////////////////////////////////////////////////////////////
			/// Constructor takes an error message.
			/// @param error the error message
			///
			/// @author mickem
			nrpe_exception(std::wstring error) : error_(error) {}

			//////////////////////////////////////////////////////////////////////////
			/// Retrieve the error message from the exception.
			/// @return the error message
			///
			/// @author mickem
			std::wstring what() const { return error_; }

		};

		class server : private boost::noncopyable {
		public:
			struct connection_info {
				connection_info(boost::shared_ptr<nrpe::server::handler> request_handler_) : request_handler(request_handler_) {}
				std::string address;
				std::string port;
				std::size_t thread_pool_size;
				bool use_ssl;
				boost::shared_ptr<nrpe::server::handler> request_handler;

				std::wstring get_endpoint_str() {
					return to_wstring(address) + _T(":") + to_wstring(port);
				}
			};


		public:
			/// Construct the server to listen on the specified TCP address and port, and
			/// serve up files from the given directory.
			explicit server(connection_info info);

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
			//connection_ptr new_connection_;
			connection_ptr new_connection_;

			std::vector<boost::shared_ptr<boost::thread> > threads_;

			/// The handler for all incoming requests.
			boost::shared_ptr<nrpe::server::handler> request_handler_;

			boost::asio::ssl::context context_;

			bool use_ssl_;

			/// The strand for handleTcpAccept(), handleSslAccept() and handleStop()
			boost::asio::strand accept_strand_;

		};

	} // namespace server
} // namespace nrpe
