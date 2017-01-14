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

#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include <nsca/nsca_packet.hpp>
//#include <cryptopp/cryptopp.hpp>

#include "handler.hpp"

namespace nsca {
	namespace server {
		class parser : public boost::noncopyable {
			unsigned int payload_length_;
			unsigned int packet_length_;

			std::string buffer_;
			boost::shared_ptr<nsca::server::handler> handler_;
		public:
			parser(unsigned int payload_length)
				: payload_length_(payload_length)
				, packet_length_(nsca::length::get_packet_length(payload_length)) {}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
				int count = packet_length_ - buffer_.size();
				for (; count > 0 && begin != end; ++begin, --count)
					buffer_.push_back(*begin);
				return boost::make_tuple(buffer_.size() >= packet_length_, begin);
			}

			void decrypt(nscp::encryption::engine &encryption) {
				encryption.decrypt_buffer(buffer_);
			}
			nsca::packet parse() {
				nsca::packet packet(payload_length_);
				packet.parse_data(buffer_.c_str(), buffer_.size());
				buffer_.clear();
				return packet;
			}
			std::string get_buffer() const {
				return buffer_;
			}
			std::string::size_type size() {
				return buffer_.size();
			}
			void reset() {
				buffer_.clear();
			}
		};
	}// namespace server
} // namespace nsca