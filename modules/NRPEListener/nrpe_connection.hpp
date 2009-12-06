#pragma once
#include "nrpe_handler.hpp"
#include "nrpe_parser.hpp"
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
/*
#include "reply.hpp"
#include "request.hpp"
#include "request_handler.hpp"
#include "request_parser.hpp"
*/
namespace nrpe {
	namespace server {

		/// Represents a single connection from a client.
		class connection : public boost::enable_shared_from_this<connection>, private boost::noncopyable {
		public:
			/// Construct a connection with the given io_service.
			explicit connection(boost::asio::io_service& io_service, nrpe::server::handler& handler);
			virtual ~connection() {
				std::cout << "Destroying... connecyion..." << std::endl;
			}

			/// Get the socket associated with the connection.
			boost::asio::ip::tcp::socket& socket();

			/// Start the first asynchronous operation for the connection.
			void start();

		private:
			/// Handle completion of a read operation.
			void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);

			/// Handle completion of a write operation.
			void handle_write(const boost::system::error_code& e);

			/// Strand to ensure the connection's handlers are not called concurrently.
			boost::asio::io_service::strand strand_;

			/// Socket for the connection.
			boost::asio::ip::tcp::socket socket_;

			/// The handler used to process the incoming request.
			//request_handler& request_handler_;

			typedef boost::array<char, 8192> buffer_type;
			/// Buffer for incoming data.
			buffer_type buffer_;

			/// The incoming request.
			//request request_;

			/// The parser for the incoming request.
			nrpe::server::handler &handler_;
			nrpe::server::parser parser_;
			//request_parser request_parser_;

			/// The reply to be sent back to the client.
			//reply reply_;
		};

		typedef boost::shared_ptr<connection> connection_ptr;

	} // namespace server
} // namespace nrpe
