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
			virtual void start_write_request(const std::vector<boost::asio::const_buffer>& response);

			/// Handle completion of a read operation.
			//void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);

			/// Handle completion of a write operation.
			//void handle_write(const boost::system::error_code& e);


			/// Socket for the connection.
			boost::asio::ip::tcp::socket socket_;
		};

/*
		namespace socket_handlers {

			class socket {
			public:
				typedef boost::asio::basic_socket<tcp,boost::asio::stream_socket_service<tcp> >  basic_socket_type;
				virtual basic_socket_type& get() = 0;
			};

			class normal_socket : public socket {
			public:
				socket::basic_socket_type& get() {
					return socket_;
				}
				normal_socket(boost::asio::io_service& io_service) 
					: socket_(io_service)
				{}
			private:
				basic_socket_type socket_;
			};
			class ssl_socket : public socket {
			public:
				socket::basic_socket_type& get() {
					return socket_.lowest_layer();
				}
				
				ssl_socket(boost::asio::io_service& io_service, boost::asio::ssl::context &context) 
					: socket_(io_service, context)
				{}
			private:
				typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_type;
				socket_type socket_;
			};

		}
		/// Represents a single connection from a client.
		class connection_base : public boost::enable_shared_from_this<connection_base>, private boost::noncopyable {
		public:
			/// Construct a connection with the given io_service.
			explicit connection_base(boost::asio::io_service& io_service, nrpe::server::socket_handlers::socket* socket, nrpe::server::handler& handler);
			virtual ~connection_base() {
				handler_.log_debug(__FILEW__, __LINE__, _T("Destroying socket..."));
			}

			/// Get the socket associated with the connection.
			nrpe::server::socket_handlers::socket::basic_socket_type& socket();

			/// Start the first asynchronous operation for the connection.
			void start();
			void start_ssl();

		private:
			/// Handle completion of a read operation.
			void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);

			/// Handle completion of a write operation.
			void handle_write(const boost::system::error_code& e);

			void handle_handshake(const boost::system::error_code& error);

			/// Strand to ensure the connection's handlers are not called concurrently.
			boost::asio::io_service::strand strand_;

			/// Socket for the connection.
			//socket_handler socket_;
			boost::shared_ptr<nrpe::server::socket_handlers::socket> socket_;

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
*/

/*
		class ssl_connection : public boost::enable_shared_from_this<ssl_connection>, private boost::noncopyable {
		private:
			typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
			connection_ptr connection_;
		public:

			explicit ssl_connection(boost::asio::io_service& io_service, boost::asio::ssl::context &context, nrpe::server::handler& handler);
			virtual ~ssl_connection() {
				handler_.log_debug(__FILEW__, __LINE__, _T("Destroying SSL socket..."));
			}

			ssl_socket::lowest_layer_type& socket();
			void start();

		private:
			ssl_socket socket_;
			nrpe::server::handler &handler_;

		};

		typedef boost::shared_ptr<ssl_connection> ssl_connection_ptr;
*/

	} // namespace server
} // namespace nrpe
