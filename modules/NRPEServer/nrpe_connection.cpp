#include "stdafx.h"

#include "nrpe_connection.hpp"
#include <vector>
#include <boost/bind.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/utility.hpp>
#include <boost/tuple/tuple.hpp>

namespace nrpe {
	namespace server {

		connection::connection(boost::asio::io_service& io_service, nrpe::server::handler& handler)
			: strand_(io_service)
			, socket_(io_service)
			, handler_(handler)
			, parser_(0)
		{}

		boost::asio::ip::tcp::socket& connection::socket() {
			return socket_;
		}

		void connection::start() {
			parser_.set_payload_length(handler_.get_payload_length());
			socket_.async_read_some(boost::asio::buffer(buffer_),
				strand_.wrap(
					boost::bind(&connection::handle_read, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred)
				));
		}

		void connection::handle_read(const boost::system::error_code& e, std::size_t bytes_transferred) {
			if (!e) {
				bool result;
				buffer_type::iterator begin = buffer_.begin();
				buffer_type::iterator end = buffer_.begin() + bytes_transferred;
				while (begin != end) {
					buffer_type::iterator old_begin = begin;
					boost::tie(result, begin) = parser_.digest(begin, end);
					if (begin == old_begin) {
						handler_.create_error(_T("Something strange happened..."));
						return;
					}
					if (result) {
						nrpe::packet response;
						try {
							nrpe::packet request = parser_.parse();
							response = handler_.handle(request);
						} catch (nrpe::nrpe_packet_exception &e) {
							response = handler_.create_error(e.getMessage());
						} catch (...) {
							response = handler_.create_error(_T("Unknown error handling packet"));
						}
						boost::asio::async_write(socket_, boost::asio::buffer(response.create_buffer(), response.get_packet_length()),
							strand_.wrap(
							boost::bind(&connection::handle_write, shared_from_this(),
							boost::asio::placeholders::error)));
					}
				}
// 				if (result) {
// 					/*
// 					request_handler_.handle_request(request_, reply_);
// 					boost::asio::async_write(socket_, reply_.to_buffers(),
// 						strand_.wrap(
// 						boost::bind(&connection::handle_write, shared_from_this(),
// 						boost::asio::placeholders::error)));
// 						*/
// 				} else if (!result) {
// // 					reply_ = reply::stock_reply(reply::bad_request);
// // 					boost::asio::async_write(socket_, reply_.to_buffers(),
// // 						strand_.wrap(
// // 						boost::bind(&connection::handle_write, shared_from_this(),
// // 						boost::asio::placeholders::error)));
// 				} else {
// // 					socket_.async_read_some(boost::asio::buffer(buffer_),
// // 						strand_.wrap(
// // 						boost::bind(&connection::handle_read, shared_from_this(),
// // 						boost::asio::placeholders::error,
// // 						boost::asio::placeholders::bytes_transferred)));
// 				}
// 				socket_.async_read_some(boost::asio::buffer(buffer_),
// 					strand_.wrap(
// 						boost::bind(&connection::handle_read, shared_from_this(),
// 						boost::asio::placeholders::error,
// 						boost::asio::placeholders::bytes_transferred)));
			}

			// If an error occurs then no new asynchronous operations are started. This
			// means that all shared_ptr references to the connection object will
			// disappear and the object will be destroyed automatically after this
			// handler returns. The connection class's destructor closes the socket.
		}

		void connection::handle_write(const boost::system::error_code& e) {
			if (!e) {
				// Initiate graceful connection closure.
				boost::system::error_code ignored_ec;
				socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			}
		}

	} // namespace server
} // namespace nrpe
