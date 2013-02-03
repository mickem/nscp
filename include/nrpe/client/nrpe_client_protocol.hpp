#pragma once

#include <nrpe/packet.hpp>
#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <iostream>

using boost::asio::ip::tcp;

namespace nrpe {
	namespace client {
		class protocol : public boost::noncopyable {
		public:
			// traits
			typedef std::vector<char> read_buffer_type;
			typedef std::vector<char> write_buffer_type;
			typedef nrpe::packet request_type;
			typedef nrpe::packet response_type;
			typedef socket_helpers::client::client_handler client_handler;

		private:
			std::vector<char> buffer_;
			unsigned int  payload_length_;
			boost::shared_ptr<client_handler> handler_;

			enum state {
				none,
				connected,
				has_request,
				sent_response,
				done
			};
			state current_state_;

			inline void set_state(state new_state) {
				current_state_ = new_state;
			}
		public:
			protocol(boost::shared_ptr<client_handler> handler) : handler_(handler), current_state_(none) {}
			virtual ~protocol() {}

			void on_connect() {
				set_state(connected);
			}
			void prepare_request(request_type &packet) {
				set_state(has_request);
				payload_length_ = packet.get_payload_length();
				buffer_ =  packet.get_buffer();
			}

			write_buffer_type& get_outbound() {
				return buffer_;
			}
			read_buffer_type& get_inbound() {
				return buffer_;
			}

			response_type get_timeout_response() {
				return nrpe::packet::unknown_response("Failed to read data");
			}
			response_type get_response() {
				return nrpe::packet(&buffer_[0], buffer_.size());
			}
			bool has_data() {
				return current_state_ == has_request;
			}
			bool wants_data() {
				return current_state_ == sent_response;
			}

			bool on_read(std::size_t bytes_transferred) {
				bytes_transferred;
				set_state(connected);
				return true;
			}
			bool on_write(std::size_t bytes_transferred) {
				bytes_transferred;
				set_state(sent_response);
				return true;
			}
			bool on_read_error(const boost::system::error_code& e) {
				e;
				return false;
			}
		};
	}
}