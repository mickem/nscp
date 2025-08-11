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
#include <nrpe/packet.hpp>
#include <swap_bytes.hpp>

namespace nrpe {
namespace server {
class parser : public boost::noncopyable {
  unsigned int payload_length_;
  std::vector<char> buffer_;

 public:
  explicit parser(const unsigned int payload_length) : payload_length_(payload_length) {}

  int16_t read_version() const {
    if (buffer_.size() < length::get_min_header_length()) {
      return -1;
    }
    auto p = reinterpret_cast<const data::packet_header*>(buffer_.data());
    return swap_bytes::ntoh<int16_t>(p->packet_version);
  }
  int32_t read_len() const {
    if (buffer_.size() < sizeof(data::packet_v3)) {
      return -1;
    }
    const data::packet_v3* p = reinterpret_cast<const data::packet_v3*>(buffer_.data());
    return swap_bytes::ntoh<int32_t>(p->buffer_length);
  }
  std::size_t get_packet_length_v2() const { return length::get_packet_length_v2(payload_length_); }
  std::size_t get_packet_length_v3() const {
    if (buffer_.size() > length::get_packet_length_v3(0)) {
      if (read_version() == data::version3) {
        return length::get_packet_length_v3(read_len());
      } else {
        return length::get_packet_length_v4(read_len());
      }
    }
    return length::get_packet_length_v3(payload_length_);
  }

  template <typename InputIterator>
  boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
    int16_t v = read_version();
    if (v == -1 || v == 2) {
      for (std::size_t count = get_packet_length_v2() - buffer_.size(); count > 0 && begin != end; ++begin, --count) buffer_.push_back(*begin);
    } else if (v == data::version3 || v == data::version4) {
      std::size_t count = get_packet_length_v3() - buffer_.size();
      for (; count > 0 && begin != end; ++begin, --count) buffer_.push_back(*begin);
    }

    v = read_version();
    const std::size_t packet_length = read_version() >= 3 ? get_packet_length_v3() : get_packet_length_v2();
    if (packet_length < 1024 || packet_length > 2048 * 1024) {
      return boost::make_tuple(true, begin);
    }

    return boost::make_tuple(buffer_.size() >= packet_length, begin);
  }

  packet parse() {
    packet packet(buffer_, payload_length_);
    buffer_.clear();
    return packet;
  }
  void reset() { buffer_.clear(); }
};
}  // namespace server
}  // namespace nrpe