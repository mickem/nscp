#include "connection.hpp"
#include "tcp_connection.hpp"
#include "ssl_connection.hpp"
#include <vector>
#include <boost/bind.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/utility.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/asio/ssl.hpp>

#include <protobuf/ipc.pb.h>
#include <protobuf/plugin.pb.h>

namespace nscp {
	namespace server {

		connection::connection(boost::asio::io_service& io_service, boost::shared_ptr<nscp::server::server_handler> handler)
			: strand_(io_service)
			, handler_(handler)
			, parser_(handler)
			, timer_(io_service)
		{}

		connection::~connection() {}

		connection* factories::create(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<nscp::server::server_handler> handler, bool use_ssl) {
			if (use_ssl)
				return create_ssl(io_service, context, handler);
			return create_tcp(io_service, handler);
		}
		connection* factories::create_tcp(boost::asio::io_service& io_service, boost::shared_ptr<nscp::server::server_handler> handler) {
			return new tcp_connection(io_service, handler);
		}
		connection* factories::create_ssl(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<nscp::server::server_handler> handler) {
			return new ssl_connection(io_service, context, handler);
		}

		void connection::start() {
			//handler_->log_debug(__FILE__, __LINE__, _T("starting data connection"));
			process_helper helper(&nscp::server::parser::digest_signature, &connection::process_signature);
			start_read_request(buffer_, 30, helper);
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


		void connection::handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred, process_helper helper) {
			if (!e) {
				if (!helper.digester || !helper.processor) {
					handler_->log_error(__FILE__, __LINE__, _T("Missing helper function pointers"));
					return;
				}
				bool more = false;
				bool result;
				buffer_type::iterator begin = buffer_.begin();
				buffer_type::iterator end = buffer_.begin() + bytes_transferred;
				while (begin != end) {
					buffer_type::iterator old_begin = begin;
					boost::tie(result, begin) = helper.digester(&parser_, begin, end);
					if (begin == old_begin) {
						handler_->create_error(_T("Digester failed to process any data"));
						return;
					}
					if (result)
						boost::tie(more, helper) = helper.processor(this);
					else
						more = true;
				}
				if (more) {
					handler_->log_debug(__FILE__, __LINE__, _T("WAIT FOR MORE DATA"));
					start_read_request(buffer_, 30, helper);
				} else {
					unsigned int count = outbound_queue_.size();
					outbound_queue_.push_front(nscp::factory::create_envelope_response(count));
					BOOST_FOREACH(nscp::packet &chunk, outbound_queue_) {
						chunk.signature.additional_packet_count = count--;
						std::string s = chunk.write_string();
						handler_->log_debug(__FILE__, __LINE__, _T(">>>") + chunk.signature.to_wstring());
						response_buffers_.push_back(buf(s));
					}
					outbound_queue_.clear();
					start_write_request(response_buffers_);
				}
			} else {
				handler_->log_debug(__FILE__, __LINE__, _T("Failed to read data: ") + utf8::cvt<std::wstring>(e.message()));
			}
		}

		boost::tuple<bool, connection::process_helper> connection::process_signature() {
			sig = parser_.parse_signature();

			handler_->log_debug(__FILE__, __LINE__, _T("<<<") + sig.to_wstring());
			if (sig.header_length > 0) {
				// @todo read header
				handler_->log_error(__FILE__, __LINE__, _T("Headers is currently not supported..."));
			}
			return boost::make_tuple(true, process_helper(boost::bind(&nscp::server::parser::digest_payload, _1, _2, _3, sig), &connection::process_payload));
		}

		boost::tuple<bool, connection::process_helper> connection::process_payload() {
			nscp::packet packet(sig);
			parser_.parse_payload(packet);
			if (sig.payload_type == nscp::data::envelope_request) {
				NSCPIPC::RequestEnvelope envelope;
				envelope.ParseFromString(packet.payload);
			} else {
				std::list<nscp::packet> result = handler_->process(packet);
				outbound_queue_.insert(outbound_queue_.end(), result.begin(), result.end());
			}
			return boost::make_tuple(sig.additional_packet_count > 0, process_helper(&nscp::server::parser::digest_signature, &connection::process_signature));
		}


		boost::asio::const_buffer connection::buf(const std::string s) {
			buffers_.push_back(s);
			return boost::asio::buffer(buffers_.back());
		}

		void connection::handle_write_response(const boost::system::error_code& e) {
			if (!e) {
				handler_->log_debug(__FILE__, __LINE__, _T("Done sending data"));
				cancel_timer();
				// Initiate graceful connection closure.
				boost::system::error_code ignored_ec;
				socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			}
		}
	} // namespace server
} // namespace nscp
