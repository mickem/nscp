// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/noncopyable.hpp>
#include <boost/tuple/tuple.hpp>
#include <net/check_nt/packet.hpp>

#include "handler.hpp"

namespace check_nt {
namespace server {
class parser : public boost::noncopyable {
  // check_nt requests are short (`<password>&<cmd>&<args>`). Cap the
  // per-connection buffer so a peer cannot pin memory by trickling bytes.
  // 4 KiB is generous - real requests are well under 1 KiB.
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
    // The legacy wire format has no terminator: the real check_nt client
    // sends `<password>&<cmd>&<args>` with no trailing newline and waits for
    // the response, so end-of-chunk is end-of-request. (Requiring a newline
    // here made every nagios-plugins check_nt hang until its socket timeout.)
    // The newline path above additionally serves line-oriented clients and
    // keeps any pipelined bytes after a mid-buffer newline intact.
    return boost::make_tuple(!buffer_.empty(), begin);
  }

  check_nt::packet parse() {
    check_nt::packet packet(buffer_);
    buffer_.clear();
    return packet;
  }
};
}  // namespace server
}  // namespace check_nt