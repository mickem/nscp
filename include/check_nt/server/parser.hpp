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

#include <check_nt/packet.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include "handler.hpp"

namespace check_nt {
	namespace server {
		class parser : public boost::noncopyable {
			std::vector<char> buffer_;
		public:
			parser() {}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
				for (; begin != end; ++begin) {
					buffer_.push_back(*begin);
					if (*begin == '\n') {
						break;
					}
				}
				return boost::make_tuple(true, begin);
			}

			check_nt::packet parse() {
				check_nt::packet packet(buffer_);
				buffer_.clear();
				return packet;
			}
		};
	}// namespace server
} // namespace check_nt