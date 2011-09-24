#include <vector>

#include <boost/bind.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/utility.hpp>
#include <boost/tuple/tuple.hpp>


#include "connection.hpp"

#include <unicode_char.hpp>
#include <strEx.h>

namespace str = nscp::helpers;

namespace nsca {
	namespace server {

		connection::connection(boost::asio::io_service& io_service, boost::shared_ptr<nsca::server::handler> handler)
			: strand_(io_service)
			, handler_(handler)
			, parser_(handler)
			, timer_(io_service)
			, socket_(io_service)
		{}

		connection::~connection() {}

		void connection::start() {
			handler_->log_debug(__FILE__, __LINE__, _T("starting data connection..."));
			std::vector<boost::asio::const_buffer> buffers;
			nsca::iv_packet packet(nsca_encrypt::generate_transmitted_iv());
			buffers.push_back(buf(packet.get_buffer()));
			handler_->log_debug(__FILE__, __LINE__, _T("About to write: ") + strEx::itos(packet.get_buffer().size()));
			start_write_request(buffers, 30);
		}

		void connection::set_timeout(int seconds) {
			timer_.expires_from_now(boost::posix_time::seconds(seconds));
			timer_.async_wait(boost::bind(&connection::timeout, shared_from_this(), boost::asio::placeholders::error));  
		}

		void connection::cancel_timer() {
			timer_.cancel();
		}

		void connection::stop() {
			socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
			socket_.close();
			handler_->log_debug(__FILE__, __LINE__, _T("stopped data connection"));
		}


		void connection::timeout(const boost::system::error_code& e) {
			if (e != boost::asio::error::operation_aborted) {
				handler_->log_debug(__FILE__, __LINE__, _T("Timeout <<<-"));
				boost::system::error_code ignored_ec;
				socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			}
		}
		void connection::handle_write_response(const boost::system::error_code& e, std::size_t bytes_transferred) {
			handler_->log_error(__FILE__, __LINE__, _T("Wrote data (server): ") + strEx::itos(bytes_transferred));
			if (!e)
				start_read_request(buffer_);
		}


		void connection::handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
			if (!e) {
				handler_->log_error(__FILE__, __LINE__, _T("Got data (server): ") + strEx::itos(bytes_transferred));
				bool result;
				buffer_type::iterator begin = buffer_.begin();
				buffer_type::iterator end = buffer_.begin() + bytes_transferred;
				while (begin != end) {
					buffer_type::iterator old_begin = begin;
					boost::tie(result, begin) = parser_.digest(begin, end);
					if (begin == old_begin) {
						handler_->log_error(__FILE__, __LINE__, _T("Something strange happened..."));
						return;
					}
					if (result) {
						nsca::packet response;
						try {
							// TODO decrypt data here...
							//NSC_DEBUG_MSG(_T("Encrypting using: ") + strEx::itos(encryption_method));
							nsca::packet request = parser_.parse();
							handler_->handle(request);
						} catch (nsca::nsca_exception &e) {
							handler_->log_error(__FILE__, __LINE__, str::to_wstring(e.what()));
						} catch (...) {
							handler_->log_error(__FILE__, __LINE__, _T("Unknown error handling packet"));
						}
						cancel_timer();
						handler_->log_error(__FILE__, __LINE__, _T("Done reading packet (server), shutting down..."));
						// Initiate graceful connection closure.
						boost::system::error_code ignored_ec;
						socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
					}
				}
			} else {
				handler_->log_debug(__FILE__, __LINE__, _T("Failed to read request"));
			}
		}
		void connection::start_read_request(buffer_type &buffer) {
			socket_.async_read_some(
				boost::asio::buffer(buffer),
				strand_.wrap(boost::bind(&connection::handle_read_request, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
			);
		}
		void connection::start_write_request(const std::vector<boost::asio::const_buffer>& response, int timeout) {
			set_timeout(timeout);
			boost::asio::async_write(socket_, response,
				strand_.wrap(boost::bind(&connection::handle_write_response, shared_from_this(),boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
			);
		}

		boost::asio::const_buffer connection::buf(const std::string s) {
			buffers_.push_back(s);
			return boost::asio::buffer(buffers_.back());
		}

	} // namespace server
} // namespace nsca
