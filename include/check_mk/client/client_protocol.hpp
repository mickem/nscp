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

#include <check_mk/data.hpp>
#include <check_mk/parser.hpp>
#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <iostream>

using boost::asio::ip::tcp;

namespace check_mk {
	namespace client {
		class protocol : public boost::noncopyable {
		public:
			// traits
			typedef std::vector<char> read_buffer_type;
			typedef std::vector<char> write_buffer_type;
			typedef std::string request_type;
			typedef check_mk::packet response_type;
			typedef socket_helpers::client::client_handler client_handler;
			static const bool debug_trace = false;

		private:
			enum state {
				none,
				connected,
				wants_response,
				done
			};

			boost::shared_ptr<client_handler> handler_;
			state current_state_;

			std::vector<char> read_buffer_;
			std::string data_buffer_;

			inline void set_state(state new_state) {
				current_state_ = new_state;
			}
		public:
			protocol(boost::shared_ptr<client_handler> handler)
				: handler_(handler)
				, current_state_(none) {
				read_buffer_.resize(40960);
			}
			virtual ~protocol() {}

			void on_connect() {
				set_state(connected);
			}
			void prepare_request(request_type &) {
				set_state(wants_response);
			}

			write_buffer_type& get_outbound() {
				return read_buffer_;
			}
			read_buffer_type& get_inbound() {
				return read_buffer_;
			}

			response_type get_timeout_response() {
				return check_mk::packet();
			}
			response_type get_response() {
				check_mk::packet ret;
				ret.read(data_buffer_);
				return ret;
			}
			bool has_data() {
				return false;
			}
			bool wants_data() {
				return current_state_ == wants_response;
			}

			bool on_read_error(const boost::system::error_code& e) {
				handler_->log_debug(__FILE__, __LINE__, "*** GOT ERROR: " + e.message());
				set_state(done);
				return true;
			}
			bool on_read(std::size_t bytes_transferred) {
				read_buffer_type::iterator begin = read_buffer_.begin();
				read_buffer_type::iterator end = read_buffer_.begin() + bytes_transferred;
				data_buffer_.insert(data_buffer_.end(), begin, end);
				return true;
			}
			bool on_write(std::size_t) {
				return true;
			}
		};
	}
}