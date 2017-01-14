/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <socket/clients/http/http_packet.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>

using boost::asio::ip::tcp;

namespace http {
	namespace client {


		class protocol : public boost::noncopyable {
		public:
			// traits
			typedef std::vector<char> read_buffer_type;
			typedef std::vector<char> write_buffer_type;
			typedef http::packet request_type;
			typedef http::packet response_type;
			typedef socket_helpers::client::client_handler client_handler;
			static const bool debug_trace = false;

		private:
			std::vector<char> buffer_;
			boost::shared_ptr<client_handler> handler_;
			request_type packet_;
			std::vector<char> responseData_;

			enum state {
				none,
				connected,
				has_data_to_send,
				wants_data_to_read,
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
				packet_ = packet;
				prepare_to_send();
			}

			write_buffer_type& get_outbound() {
				return buffer_;
			}
			read_buffer_type& get_inbound() {
				return buffer_;
			}

			response_type get_timeout_response() {
				return http::packet::create_timeout("Failed to read data");
			}
			response_type get_response() {
				return response_type(responseData_);
			}
			bool has_data() {
				return current_state_ == has_data_to_send;
			}
			bool wants_data() {
				return current_state_ == wants_data_to_read;
			}

			bool on_read(std::size_t) {
				if (current_state_ == wants_data_to_read) {
					responseData_.insert(responseData_.end(), buffer_.begin(), buffer_.end());
					return true;
				}
				set_state(done);
				return true;
			}
			bool prepare_to_send() {
				set_state(has_data_to_send);
				buffer_ = packet_.get_packet();
				return true;
			}
			bool on_write(std::size_t) {
				set_state(wants_data_to_read);
				return false;
			}
			bool on_read_error(const boost::system::error_code& e) {
				if (current_state_ == wants_data_to_read) {
					set_state(done);
					return true;
				}
				return false;
			}
		};
	}
}