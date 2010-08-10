#include "ssl_connection.hpp"
#include <vector>
#include <boost/bind.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/utility.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/asio/ssl.hpp>


namespace nrpe {
	namespace server {

		ssl_connection::ssl_connection(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<nrpe::server::handler> handler)
			: connection(io_service, handler)
			, socket_(io_service, context)
		{}


		boost::asio::ip::tcp::socket& ssl_connection::socket() {
			return socket_.next_layer();
		}

		void ssl_connection::stop() {
			handler_->log_debug(__FILEW__, __LINE__, _T("stopped data connection"));
		}

		void ssl_connection::start() {
			handler_->log_debug(__FILEW__, __LINE__, _T("starting ssl_connection"));
			socket_.async_handshake(boost::asio::ssl::stream_base::server,
				strand_.wrap(
					boost::bind(&connection::handle_handshake, shared_from_this(), boost::asio::placeholders::error)
					)
				);
			handler_->log_debug(__FILEW__, __LINE__, _T("starting ssl_connection (started)"));
		}

		void ssl_connection::handle_handshake(const boost::system::error_code& error) {
			handler_->log_debug(__FILEW__, __LINE__, _T("handle_handshake ssl_connection"));
			if (!error)
				connection::start();
			else {
				handler_->log_error(__FILEW__, __LINE__, _T("Failed to establish secure connection: ") + to_wstring(error.message()));
				//ConnectionManager_.stop(shared_from_this());
			}
		}
		void ssl_connection::start_read_request(buffer_type &buffer, int timeout) {
			set_timeout(timeout);
			socket_.async_read_some(
				boost::asio::buffer(buffer),
				strand_.wrap(
					boost::bind(&connection::handle_read_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					)
				);
		}

		void ssl_connection::start_write_request(const std::vector<boost::asio::const_buffer>& response) {
			boost::asio::async_write(socket_, response,
				strand_.wrap(
					boost::bind(&connection::handle_write_response, shared_from_this(),boost::asio::placeholders::error)
					)
				);
		}

	} // namespace server
} // namespace nrpe
