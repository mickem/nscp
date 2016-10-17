/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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