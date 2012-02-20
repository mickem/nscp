#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "connection.hpp"
#include "handler.hpp"
#include "parser.hpp"

namespace nrpe {
	using boost::asio::ip::tcp;

	namespace server {

		class tcp_connection : public connection {
		public:
			/// Construct a connection with the given io_service.
			explicit tcp_connection(boost::asio::io_service& io_service, boost::shared_ptr<nrpe::server::handler> handler);

			/// Get the socket associated with the connection.
			virtual boost::asio::ip::tcp::socket& socket();

			virtual void stop();

		protected:

			virtual void start_read_request(connection::buffer_type &buffer, int timeout);
			void continue_read_request(buffer_type &buffer);
			virtual void start_write_request(const std::vector<boost::asio::const_buffer>& response);

			/// Handle completion of a read operation.
			//void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);

			/// Handle completion of a write operation.
			//void handle_write(const boost::system::error_code& e);


			/// Socket for the connection.
			boost::asio::ip::tcp::socket socket_;
		};
	} // namespace server
} // namespace nrpe
