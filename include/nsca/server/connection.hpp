#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "handler.hpp"
#include "parser.hpp"

namespace nsca {
	using boost::asio::ip::tcp;

	namespace server {

		class connection : public boost::enable_shared_from_this<connection>, private boost::noncopyable {
		public:
			typedef boost::array<char, 8192> buffer_type;

			/// Construct a connection with the given io_service.
			explicit connection(boost::asio::io_service& io_service, boost::shared_ptr<nsca::server::handler> handler);

			/// Get the socket associated with the connection.
			//virtual boost::asio::ip::tcp::socket& socket() = 0;
			boost::asio::ip::tcp::socket& socket() {
				return socket_;
			}


			/// Start the first asynchronous operation for the connection.
			void start();

			/// Stop all asynchronous operations associated with the connection.
			void stop();

			virtual ~connection();

			void start_read_request(buffer_type &buffer);
			void start_write_request(const std::vector<boost::asio::const_buffer>& response, int timeout);

			void handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred);
			void handle_write_response(const boost::system::error_code& e, std::size_t bytes_transferred);
			virtual void handle_handshake(const boost::system::error_code& e) {} 

		protected:

			void set_timeout(int seconds);
			void cancel_timer();
			void timeout(const boost::system::error_code& e);

			boost::asio::const_buffer buf(const std::string s);


			/// Strand to ensure the connection's handlers are not called concurrently.
			boost::asio::io_service::strand strand_;

			/// The handler used to process the incoming request.
			boost::shared_ptr<nsca::server::handler> handler_;

			/// Buffer for incoming data.
			buffer_type buffer_;

			/// The parser for the incoming request.
			nsca::server::parser parser_;

			/// Timer for reading data.
			boost::asio::deadline_timer timer_;

			std::list<std::string> buffers_;

			boost::asio::ip::tcp::socket socket_;

		};

		//typedef connection_base connection;
		typedef boost::shared_ptr<connection> connection_ptr;

		class factories {
		public:
			static connection* create(boost::asio::io_service& io_service, boost::shared_ptr<nsca::server::handler> handler);
		};

	} // namespace server
} // namespace nsca
