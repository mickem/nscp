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