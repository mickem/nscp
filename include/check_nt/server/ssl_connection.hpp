#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <check_nt/server/parser.hpp>
#include "handler.hpp"
#include "connection.hpp"

namespace check_nt {
	using boost::asio::ip::tcp;

	namespace server {

		class ssl_connection : public connection {
		public:

			ssl_connection(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<check_nt::server::handler> handler);

			/// Get the socket associated with the connection.
			virtual boost::asio::ip::tcp::socket& socket();
			typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;


			virtual void start();
			virtual void stop();

		protected:

			virtual void start_read_request(connection::buffer_type &buffer, int timeout);
			virtual void start_write_request(const std::vector<boost::asio::const_buffer>& response);
			void handle_handshake(const boost::system::error_code& error);

			/// Socket for the connection.
			ssl_socket socket_;
		};
	} // namespace server
} // namespace nrpe
