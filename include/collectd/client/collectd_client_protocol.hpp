#pragma once

#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <iostream>

using boost::asio::ip::tcp;

namespace collectd {
	namespace client {
		template<class handler_type>
		class protocol : public boost::noncopyable {
		public:
			// traits
			typedef std::vector<char> read_buffer_type;
			typedef std::vector<char> write_buffer_type;
			typedef const collectd::packet request_type;
			typedef bool response_type;
			typedef handler_type client_handler;
			static const bool debug_trace = false;

		private:
			std::vector<char> iv_buffer_;
			std::vector<char> packet_buffer_;
			boost::shared_ptr<client_handler> handler_;
			int time_;
			collectd::packet packet_;

			enum state {
				none,
				connected,
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
				set_state(connected);
				packet_ = packet;
			}

			write_buffer_type& get_outbound() {
				std::string str;
				packet_.get_buffer(str);
				packet_buffer_ = std::vector<char>(str.begin(), str.end());
				return packet_buffer_;
			}
			read_buffer_type& get_inbound() {
				iv_buffer_ = std::vector<char>();
				return iv_buffer_;
			}

			response_type get_timeout_response() {
				return false;
			}
			response_type get_response() {
				return true;
			}
			bool has_data() {
				return true; // current_state_ == has_request || current_state_ == got_iv;
			}
			bool wants_data() {
				return false; // current_state_ == connected;
			}

			bool on_read(std::size_t bytes_transferred) {
				//set_state(got_iv);
				return true;
			}
			bool on_write(std::size_t bytes_transferred) {
				//set_state(sent_request);
				return true;
			}
			bool on_read_error(const boost::system::error_code&) {
				return false;
			}
		};
	}
}
