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

			std::string iv = nsca::nsca_encrypt::generate_transmitted_iv();
			handler_->log_debug(__FILE__, __LINE__, _T("Encrypting using when receiving: ") + utf8::cvt<std::wstring>(nsca::nsca_encrypt::helpers::encryption_to_string(handler_->get_encryption())) + _T(" and ") + utf8::cvt<std::wstring>(handler_->get_password()));
			encryption_instance_.encrypt_init(handler_->get_password(), handler_->get_encryption(), iv);

			nsca::iv_packet packet(iv, boost::posix_time::second_clock::local_time());
			buffers.push_back(buf(packet.get_buffer()));
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
				handler_->log_debug(__FILE__, __LINE__, _T("Timeout reading:") + strEx::itos(parser_.get_payload_lenght()));
				boost::system::error_code ignored_ec;
				socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			}
		}
		void connection::handle_write_response(const boost::system::error_code& e, std::size_t bytes_transferred) {
			handler_->log_debug(__FILE__, __LINE__, _T("Wrote data (server): ") + strEx::itos(bytes_transferred));
			if (!e)
				start_read_request(buffer_);
		}


		void connection::handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
			if (!e) {
				handler_->log_debug(__FILE__, __LINE__, _T("Recieved data (server): ") + strEx::itos(bytes_transferred));
				bool result;
				buffer_type::iterator begin = buffer_.begin();
				buffer_type::iterator end = buffer_.begin() + bytes_transferred;
				while (begin != end) {
					buffer_type::iterator old_begin = begin;
					boost::tie(result, begin) = parser_.digest(begin, end);
					if (begin == old_begin) {
						handler_->log_error(__FILE__, __LINE__, _T("Failed to digest data"));
						return;
					}
					if (result) {
						nsca::packet response;
						try {
							parser_.decrypt(encryption_instance_);
							nsca::packet request = parser_.parse();
							handler_->handle(request);
						} catch (nsca::nsca_exception &e) {
							handler_->log_error(__FILE__, __LINE__, str::to_wstring(e.what()));
						} catch (const std::exception &e) {
							handler_->log_error(__FILE__, __LINE__, str::to_wstring(e.what()));
						} catch (...) {
							handler_->log_error(__FILE__, __LINE__, _T("Unknown error handling packet"));
						}
						cancel_timer();
						// Initiate graceful connection closure.
						boost::system::error_code ignored_ec;
						socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
					}
				}
			} else {
				handler_->log_debug(__FILE__, __LINE__, _T("Failed to read request: ") + utf8::to_unicode(e.message()) + _T(" after ") + strEx::itos(parser_.size()));
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
