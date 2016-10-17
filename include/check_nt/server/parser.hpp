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