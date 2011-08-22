#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/function.hpp>

#include "handler.hpp"
#include "parser.hpp"

namespace nscp {
	using boost::asio::ip::tcp;

	namespace server {

		class connection : public boost::enable_shared_from_this<connection>, private boost::noncopyable {
		public:
			typedef boost::array<char, 8192> buffer_type;


			struct process_helper {
				typedef boost::function<void(connection*,const boost::system::error_code&, std::size_t)> reader_function;
				typedef boost::function<boost::tuple<bool, process_helper>(connection*)> process_function;
				nscp::server::parser::digest_function digester;
				process_function processor;
				process_helper(nscp::server::parser::digest_function digester, process_function processor) : digester(digester), processor(processor) {}
				process_helper() {}
			};

			/// Construct a connection with the given io_service.
			explicit connection(boost::asio::io_service& io_service, boost::shared_ptr<nscp::server::handler> handler);

			/// Get the socket associated with the connection.
			virtual boost::asio::ip::tcp::socket& socket() = 0;

			/// Start the first asynchronous operation for the connection.
			virtual void start();

			/// Stop all asynchronous operations associated with the connection.
			virtual void stop() = 0;

			virtual ~connection();

			virtual void start_read_request(buffer_type &buffer, int timeout, process_helper helper) = 0;
			virtual void start_write_request(const std::vector<boost::asio::const_buffer>& response) = 0;
			//virtual void start_handle_handsc_request(nscp::packet response) = 0;

			boost::tuple<bool, process_helper> process_signature();
			boost::tuple<bool, process_helper> process_payload();

			bool process_read_data(std::size_t bytes_transferred, process_helper helper);
			void handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred, process_helper helper);
			void handle_write_response(const boost::system::error_code& e);
			virtual void handle_handshake(const boost::system::error_code& e) {} 


		protected:

			void set_timeout(int seconds);
			void cancel_timer();
			void timeout(const boost::system::error_code& e);

			boost::asio::const_buffer buf(const std::string s);


			/// Strand to ensure the connection's handlers are not called concurrently.
			boost::asio::io_service::strand strand_;

			/// The handler used to process the incoming request.
			boost::shared_ptr<nscp::server::handler> handler_;

			nscp::data::signature_packet sig;

			/// Buffer for incoming data.
			buffer_type buffer_;

			/// The parser for the incoming request.
			nscp::server::parser parser_;

			/// Timer for reading data.
			boost::asio::deadline_timer timer_;

			std::list<std::string> buffers_;
			std::vector<boost::asio::const_buffer> response_buffers_;
			std::list<nscp::packet::nscp_chunk> outbound_queue_;

		};

		//typedef connection_base connection;
		typedef boost::shared_ptr<connection> connection_ptr;

		class factories {
		public:
			static connection* create(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<nscp::server::handler> handler, bool use_ssl);
			static connection* create_tcp(boost::asio::io_service& io_service, boost::shared_ptr<nscp::server::handler> handler);
			static connection* create_ssl(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<nscp::server::handler> handler);
		};

	} // namespace server
} // namespace nscp
