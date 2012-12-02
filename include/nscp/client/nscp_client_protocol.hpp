#pragma once

#include <nscp/packet.hpp>
#include <nscp/server/parser.hpp>
#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <iostream>

using boost::asio::ip::tcp;

namespace nscp {
	namespace client {
		class protocol : public boost::noncopyable {
		public:
			// traits
			typedef std::vector<char> read_buffer_type;
			typedef std::vector<char> write_buffer_type;
			typedef nscp::packet request_type;
			typedef nscp::packet response_type;
			typedef socket_helpers::client::client_handler client_handler;

		private:
			enum state {
				none,
				connected,
				has_request,
				sent_response,
				done
			};

			boost::shared_ptr<client_handler> handler_;
			state current_state_;

			std::vector<char> buffer_;
			nscp::server::digester digester_;

			inline void set_state(state new_state) {
				current_state_ = new_state;
			}
		public:
			protocol(boost::shared_ptr<client_handler> handler) 
				: handler_(handler)
				, current_state_(none) {}
			virtual ~protocol() {}

			void on_connect() {
				set_state(connected);
			}
			void prepare_request(request_type &packet) {
				set_state(has_request);
				buffer_  = packet.write_string();
			}

			write_buffer_type& get_outbound() {
				return buffer_;
			}
			read_buffer_type& get_inbound() {
				return buffer_;
			}

			response_type get_timeout_response() {
				return nscp::factory::create_error(_T("Failed to read data"));
			}
			response_type get_response() {
				return digester_.get_packet();
			}
			bool has_data() {
				return current_state_ == has_request;
			}
			bool wants_data() {
				return current_state_ == sent_response;
			}

			bool on_read(std::size_t bytes_transferred) {
				read_buffer_type::iterator begin = buffer_.begin();
				read_buffer_type::iterator end = buffer_.end();
				while (begin != end) {
					bool result;
					boost::tie(result, begin) = digester_.digest(begin, end);
					if (result) {
						set_state(connected);
						return true;
					}
				}
				buffer_.resize(digester_.get_next_size());
				return true;
			}
			bool on_write(std::size_t bytes_transferred) {
				set_state(sent_response);
				digester_.reset();
				buffer_.resize(digester_.get_next_size());
				return true;
			}
			bool on_read_error(const boost::system::error_code& e) {
				return false;
			}
		};
	}
}