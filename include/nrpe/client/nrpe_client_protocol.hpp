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

#include <nrpe/packet.hpp>
#include <socket/socket_helpers.hpp>
#include <socket/client.hpp>

#include <boost/shared_ptr.hpp>

using boost::asio::ip::tcp;

namespace nrpe {
	namespace client {
		class protocol : public boost::noncopyable {
		public:
			// traits
			typedef std::vector<char> read_buffer_type;
			typedef std::vector<char> write_buffer_type;
			typedef nrpe::packet request_type;
			typedef std::list<nrpe::packet> response_type;
			typedef socket_helpers::client::client_handler client_handler;
			static const bool debug_trace = false;

		private:
			std::vector<char> buffer_;
			unsigned int  payload_length_;
			boost::shared_ptr<client_handler> handler_;
			response_type responses_;

			enum state {
				none,
				connected,
				has_request,
				sent_response,
				has_more,
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
				buffer_ = packet.get_buffer();
			}

			write_buffer_type& get_outbound() {
				return buffer_;
			}
			read_buffer_type& get_inbound() {
				return buffer_;
			}

			response_type get_timeout_response() {
				response_type ret;
				ret.push_back(nrpe::packet::unknown_response("Failed to read data"));
				return ret;
			}
			response_type get_response() {
				return responses_;
			}
			bool has_data() {
				return current_state_ == has_request;
			}
			bool wants_data() {
				return current_state_ == sent_response || current_state_ == has_more;
			}

			bool on_read(std::size_t) {
				nrpe::packet packet = nrpe::packet(&buffer_[0], static_cast<unsigned int>(buffer_.size()));
				if (packet.getType() == nrpe::data::moreResponsePacket)
					set_state(has_more);
				else
					set_state(connected);
				responses_.push_back(packet);
				return true;
			}
			bool on_write(std::size_t) {
				set_state(sent_response);
				return true;
			}
			bool on_read_error(const boost::system::error_code&) {
				return false;
			}
		};
	}
}