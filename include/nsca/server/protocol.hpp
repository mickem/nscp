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

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <socket/socket_helpers.hpp>
#include <socket/server.hpp>

#include "handler.hpp"
#include "parser.hpp"

namespace nsca {
	using boost::asio::ip::tcp;

	static const int socket_bufer_size = 8096;
	//	static const bool debug_trace = true;
	struct read_protocol : public boost::noncopyable {
		static const bool debug_trace = false;

		typedef std::string outbound_buffer_type;
		typedef nsca::server::handler *handler_type;
		typedef boost::array<char, socket_bufer_size>::iterator iterator_type;

		enum state {
			none,
			connected,
			sent_iv,
			done
		};

		socket_helpers::connection_info info_;
		handler_type handler_;
		nsca::server::parser parser_;
		state current_state_;

		std::string data_;
		nscp::encryption::engine encryption_instance_;

		static boost::shared_ptr<read_protocol> create(socket_helpers::connection_info info, handler_type handler) {
			return boost::shared_ptr<read_protocol>(new read_protocol(info, handler));
		}

		read_protocol(socket_helpers::connection_info info, handler_type handler)
			: info_(info)
			, handler_(handler)
			, parser_(handler->get_payload_length())
			, current_state_(none) {}

		inline void set_state(state new_state) {
			current_state_ = new_state;
		}

		bool on_accept(boost::asio::ip::tcp::socket& socket, int count) {
			std::list<std::string> errors;
			parser_.reset();
			std::string s = socket.remote_endpoint().address().to_string();
			if (info_.allowed_hosts.is_allowed(socket.remote_endpoint().address(), errors)) {
				log_debug(__FILE__, __LINE__, "Accepting connection from: " + s);
				return true;
			} else {
				BOOST_FOREACH(const std::string &e, errors) {
					log_error(__FILE__, __LINE__, e);
				}
				log_error(__FILE__, __LINE__, "Rejected connection from: " + s);
				return false;
			}
		}

		bool on_connect() {
			set_state(connected);
			std::vector<boost::asio::const_buffer> buffers;

			std::string iv = nscp::encryption::engine::generate_transmitted_iv();
			encryption_instance_.encrypt_init(handler_->get_password(), handler_->get_encryption(), iv);

			nsca::iv_packet packet(iv, boost::posix_time::second_clock::local_time());
			data_ = packet.get_buffer();
			return true;
		}

		bool wants_data() {
			return current_state_ == sent_iv;
		}
		bool has_data() {
			return current_state_ == connected;
		}

		bool on_write() {
			set_state(sent_iv);
			return true;
		}

		bool on_read(char *begin, char *end) {
			while (begin != end) {
				bool result;
				iterator_type old_begin = begin;
				boost::tie(result, begin) = parser_.digest(begin, end);
				if (begin == old_begin) {
					log_error(__FILE__, __LINE__, "Digester failed to parse chunk, giving up.");
					return false;
				}
				if (result) {
					set_state(done);
					nsca::packet response;
					try {
						parser_.decrypt(encryption_instance_);
						nsca::packet request = parser_.parse();
						handler_->handle(request);
					} catch (const std::exception &e) {
						log_error(__FILE__, __LINE__, std::string("Exception processing request: ") + e.what());
						log_debug(__FILE__, __LINE__, "Using: encryption = " + nscp::encryption::helpers::encryption_to_string(handler_->get_encryption()) + ", password = '" + handler_->get_password() + "'");
					} catch (...) {
						log_error(__FILE__, __LINE__, "Exception processing request");
					}
					return false;
				}
			}
			return true;
		}
		std::string get_outbound() const {
			return data_;
		}

		socket_helpers::connection_info get_info() const {
			return info_;
		}

		void log_debug(std::string file, int line, std::string msg) const {
			handler_->log_debug("nsca", file, line, msg);
		}
		void log_error(std::string file, int line, std::string msg) const {
			handler_->log_error("nsca", file, line, msg);
		}
	};

	namespace server {
		typedef socket_helpers::server::server<read_protocol, socket_bufer_size> server;
	}
} // namespace nsca