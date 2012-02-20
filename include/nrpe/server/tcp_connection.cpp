#include "tcp_connection.hpp"
#include <vector>
#include <boost/bind.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/utility.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/asio/ssl.hpp>


namespace nrpe {
	namespace server {

		tcp_connection::tcp_connection(boost::asio::io_service& io_service, boost::shared_ptr<nrpe::server::handler> handler)
			: connection(io_service, handler)
			, socket_(io_service)
		{}

		boost::asio::ip::tcp::socket& tcp_connection::socket() {
			return socket_;
		}

		void tcp_connection::stop() {
			handler_->log_debug(__FILE__, __LINE__, _T("stopped data connection"));
		}

		void tcp_connection::start_read_request(buffer_type &buffer, int timeout) {
			set_timeout(timeout);
			socket_.async_read_some(
				boost::asio::buffer(buffer),
				strand_.wrap(
					boost::bind(&connection::handle_read_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					)
				);
		}
		void tcp_connection::continue_read_request(buffer_type &buffer) {
			socket_.async_read_some(
				boost::asio::buffer(buffer),
				strand_.wrap(
				boost::bind(&connection::handle_read_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
				)
				);
		}
		void tcp_connection::start_write_request(const std::vector<boost::asio::const_buffer>& response) {
			boost::asio::async_write(socket_, response,
				strand_.wrap(
					boost::bind(&connection::handle_write_response, shared_from_this(),boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					)
				);
		}

	} // namespace server
} // namespace nrpe
