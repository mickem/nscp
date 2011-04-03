#include "connection.hpp"
#include "tcp_connection.hpp"
#include "ssl_connection.hpp"
#include <vector>
#include <boost/bind.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/utility.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/asio/ssl.hpp>


namespace nrpe {
	namespace server {

		connection::connection(boost::asio::io_service& io_service, boost::shared_ptr<nrpe::server::handler> handler)
			: strand_(io_service)
			, handler_(handler)
			, parser_(handler)
			, timer_(io_service)
		{}

		connection::~connection() {}

		connection* factories::create(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<nrpe::server::handler> handler, bool use_ssl) {
			if (use_ssl)
				return create_ssl(io_service, context, handler);
			return create_tcp(io_service, handler);
		}
		connection* factories::create_tcp(boost::asio::io_service& io_service, boost::shared_ptr<nrpe::server::handler> handler) {
			return new tcp_connection(io_service, handler);
		}
		connection* factories::create_ssl(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<nrpe::server::handler> handler) {
			return new ssl_connection(io_service, context, handler);
		}

		void connection::start() {
			handler_->log_debug(__FILE__, __LINE__, _T("starting data connection"));
			start_read_request(buffer_, 30);
		}

		void connection::set_timeout(int seconds) {
			timer_.expires_from_now(boost::posix_time::seconds(seconds));
			timer_.async_wait(boost::bind(&connection::timeout, shared_from_this(), boost::asio::placeholders::error));  
		}

		void connection::cancel_timer() {
			timer_.cancel();
		}

		void connection::timeout(const boost::system::error_code& e) {
			handler_->log_debug(__FILE__, __LINE__, _T("Timeout"));
			if (e != boost::asio::error::operation_aborted) {
				handler_->log_debug(__FILE__, __LINE__, _T("Timeout <<<-"));
				boost::system::error_code ignored_ec;
				socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			}
		}


		void connection::handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
			if (!e) {
				bool result;
				buffer_type::iterator begin = buffer_.begin();
				buffer_type::iterator end = buffer_.begin() + bytes_transferred;
				while (begin != end) {
					buffer_type::iterator old_begin = begin;
					boost::tie(result, begin) = parser_.digest(begin, end);
					if (begin == old_begin) {
						handler_->create_error(_T("Something strange happened..."));
						return;
					}
					if (result) {
						nrpe::packet response;
						try {
							nrpe::packet request = parser_.parse();
							response = handler_->handle(request);
						} catch (nrpe::nrpe_packet_exception &e) {
							response = handler_->create_error(e.getMessage());
						} catch (...) {
							response = handler_->create_error(_T("Unknown error handling packet"));
						}

						std::vector<boost::asio::const_buffer> buffers;
						buffers.push_back(buf(response.get_buffer()));
						start_write_request(buffers);
						cancel_timer();
					}
				}
			} else {
				handler_->log_debug(__FILE__, __LINE__, _T("Failed to read request"));
			}
		}

		boost::asio::const_buffer connection::buf(const std::vector<char> s) {
			buffers_.push_back(s);
			return boost::asio::buffer(buffers_.back());
		}

		void connection::handle_write_response(const boost::system::error_code& e) {
			if (!e) {
				// Initiate graceful connection closure.
				boost::system::error_code ignored_ec;
				socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			}
		}
	} // namespace server
} // namespace nrpe
