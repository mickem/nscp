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

#include <boost/noncopyable.hpp>
#include <boost/tuple/tuple.hpp>
#include <net/check_nt/packet.hpp>

#include "handler.hpp"

namespace check_nt {
namespace server {
class parser : public boost::noncopyable {
  // check_nt requests are short (`<password>&<cmd>&<args>\n`). Cap the
  // per-connection buffer so a peer that never sends a newline cannot pin
  // memory by trickling bytes. 4 KiB is generous - real requests are well
  // under 1 KiB.
  static constexpr std::size_t kMaxLineBytes = 4 * 1024;
  std::vector<char> buffer_;

 public:
  parser() {}

  template <typename InputIterator>
  boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
    for (; begin != end; ++begin) {
      if (buffer_.size() >= kMaxLineBytes) {
        // Line too long: surface as "complete" so the parse() / handler
        // path produces a clean error response and the connection is closed
        // without growing the buffer further.
        return boost::make_tuple(true, begin);
      }
      buffer_.push_back(*begin);
      if (*begin == '\n') {
        ++begin;
        return boost::make_tuple(true, begin);
      }
    }
    // No newline yet and the buffer is still under the cap: signal "not
    // complete" so the protocol layer keeps reading. Previously this always
    // returned true, which meant the handler ran on partial input and any
    // bytes pipelined after a mid-buffer newline were silently discarded.
    return boost::make_tuple(false, begin);
  }

  check_nt::packet parse() {
    check_nt::packet packet(buffer_);
    buffer_.clear();
    return packet;
  }
};
}  // namespace server
}  // namespace check_nt