// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_netstring.hpp"

namespace icinga_agent {

std::string frame(const std::string &payload) { return std::to_string(payload.size()) + ":" + payload + ","; }

std::vector<std::string> netstring_reader::feed(const char *data, const std::size_t size) {
  buffer_.append(data, size);
  std::vector<std::string> out;
  std::size_t pos = 0;
  for (;;) {
    // Each payload is followed by a ','; tolerate a stray extra one.
    while (pos < buffer_.size() && buffer_[pos] == ',') ++pos;
    std::string payload;
    std::size_t next = 0;
    if (pos >= buffer_.size() || !decode_one(pos, payload, next)) break;
    out.push_back(std::move(payload));
    pos = next;
  }
  buffer_.erase(0, pos);
  return out;
}

bool netstring_reader::decode_one(const std::size_t pos, std::string &payload, std::size_t &next) const {
  const std::size_t colon = buffer_.find(':', pos);
  if (colon == std::string::npos) {
    // Still waiting for the prefix - unless it is already too long to be one.
    if (buffer_.size() - pos > kMaxPrefixDigits) throw netstring_error("netstring: length prefix too long");
    return false;
  }
  const std::size_t length = parse_length(pos, colon);
  const std::size_t payload_begin = colon + 1;
  const std::size_t terminator = payload_begin + length;
  if (buffer_.size() <= terminator) return false;
  if (buffer_[terminator] != ',') throw netstring_error("netstring: missing terminator");
  payload = buffer_.substr(payload_begin, length);
  next = terminator + 1;
  return true;
}

std::size_t netstring_reader::parse_length(const std::size_t begin, const std::size_t end) const {
  if (end == begin || end - begin > kMaxPrefixDigits) throw netstring_error("netstring: invalid length prefix");
  std::size_t length = 0;
  for (std::size_t i = begin; i < end; ++i) {
    const char c = buffer_[i];
    if (c < '0' || c > '9') throw netstring_error("netstring: invalid length prefix");
    length = length * 10 + static_cast<std::size_t>(c - '0');
    if (length > max_message_) throw netstring_error("netstring: message exceeds the size limit");
  }
  return length;
}

}  // namespace icinga_agent
