#pragma once

#include <string>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include <socket/socket_helpers.hpp>
#include <check_nt/server/connection.hpp>
#include "handler.hpp"

namespace check_nt {

	namespace server {

		class check_nt_exception {
			std::wstring error_;
		public:
			//////////////////////////////////////////////////////////////////////////
			/// Constructor takes an error message.
			/// @param error the error message
			///
			/// @author mickem
			check_nt_exception(std::wstring error) : error_(error) {}

			//////////////////////////////////////////////////////////////////////////
			/// Retrieve the error message from the exception.
			/// @return the error message
			///
			/// @author mickem
			std::wstring what() const { return error_; }

		};

		class server : private boost::noncopyable {
		public:
			struct connection_info  : public socket_helpers::connection_info {
				connection_info(boost::shared_ptr<check_nt::server::handler> request_handler_) : request_handler(request_handler_) {}
				connection_info(const connection_info &other) 
					: socket_helpers::connection_info(other)
					, request_handler(other.request_handler)
				{}
				connection_info& operator=(const connection_info &other) {
					socket_helpers::connection_info::operator=(other);
					request_handler = other.request_handler;
					return *this;
				}

				boost::shared_ptr<check_nt::server::handler> request_handler;
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

			/// The io_service used to perform asynchronous operations.
			boost::asio::io_service io_service_;

			/// Acceptor used to listen for incoming connections.
			boost::asio::ip::tcp::acceptor acceptor_;

			/// The next connection to be accepted.
			//connection_ptr new_connection_;
			connection_ptr new_connection_;

			std::vector<boost::shared_ptr<boost::thread> > threads_;

			/// The handler for all incoming requests.
			boost::shared_ptr<check_nt::server::handler> request_handler_;

			boost::asio::ssl::context context_;

			/// The strand for handleTcpAccept(), handleSslAccept() and handleStop()
			boost::asio::strand accept_strand_;

			connection_info info_;

		};

	} // namespace server
} // namespace check_nt
