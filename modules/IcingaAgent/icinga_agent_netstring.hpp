// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Icinga 2 frames every JSON-RPC message as a netstring,
// `<decimal byte length>:<payload>,` (lib/base/netstring.cpp).

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace icinga_agent {

struct netstring_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Frame one payload.
std::string frame(const std::string &payload);

// Incremental decoder. Feed it whatever the socket produced; it returns every
// complete payload and keeps the remainder for the next call.
class netstring_reader {
 public:
  // Longest length prefix accepted: 20 digits hold any 64-bit size.
  static constexpr std::size_t kMaxPrefixDigits = 20;

  explicit netstring_reader(std::size_t max_message = 64 * 1024 * 1024) : max_message_(max_message) {}

  // Throws netstring_error on a malformed stream. The connection has to be
  // dropped then: there is no way to resynchronise a netstring stream.
  std::vector<std::string> feed(const char *data, std::size_t size);
  std::size_t buffered() const { return buffer_.size(); }

 private:
  // Decode the frame starting at `pos`. True with `payload` and `next` set
  // when it is complete; false when more bytes are needed.
  bool decode_one(std::size_t pos, std::string &payload, std::size_t &next) const;
  // The length prefix in [begin, end), checked against max_message_.
  std::size_t parse_length(std::size_t begin, std::size_t end) const;

  std::string buffer_;
  std::size_t max_message_;
};

}  // namespace icinga_agent
