// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/endian/conversion.hpp>
#include <boost/tuple/tuple.hpp>
#include <net/nrpe/packet.hpp>

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
    return boost::endian::big_to_native(p->packet_version);
  }
  // The advertised payload length is complete as soon as the fixed v3/v4
  // header has arrived. That is buffer_offset_v3 (16) bytes - not
  // sizeof(packet_v3) (20), which also counts the trailing buffer[1] plus
  // its padding, i.e. payload rather than header. Waiting for 20 bytes made
  // read_len() report "unknown" for a complete v4 header, and for an empty
  // v4 payload it never became known at all.
  int32_t read_len() const {
    if (buffer_.size() < data::buffer_offset_v3) {
      return -1;
    }
    const data::packet_v3* p = reinterpret_cast<const data::packet_v3*>(buffer_.data());
    return boost::endian::big_to_native(p->buffer_length);
  }
  std::size_t get_packet_length_v2() const { return length::get_packet_length_v2(payload_length_); }
  // Only meaningful once has_v3_header() is true; before that it falls back
  // to the configured payload length, which is a guess and must not be used
  // to decide whether a packet is complete.
  std::size_t get_packet_length_v3() const {
    if (has_v3_header()) {
      if (read_version() == data::version3) {
        return length::get_packet_length_v3(read_len());
      } else {
        return length::get_packet_length_v4(read_len());
      }
    }
    return length::get_packet_length_v3(payload_length_);
  }
  bool has_v3_header() const { return buffer_.size() >= data::buffer_offset_v3; }

  // Hard cap on per-connection buffer growth. The decoder in packet.hpp
  // accepts payloads up to 1 MiB inclusive; the wire packet is the payload
  // plus a small fixed-size v3 header (~16 B), so the buffer cap has to
  // sit at `1 MiB + header` or a legitimate 1 MiB-payload submission would
  // be rejected at the parser layer before it ever reaches the decoder.
  // The cap still bounds slow-loris / trickle attacks at slightly above
  // 1 MiB per connection (default operator payload is 1 KiB, rarely raised
  // past 64 KiB, so legitimate traffic sits well inside this).
  static constexpr std::size_t kMaxPayloadBytes = 1024u * 1024u;
  static constexpr std::size_t kMaxBufferBytes = sizeof(data::packet_v3) + kMaxPayloadBytes;

  template <typename InputIterator>
  boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
    int16_t v = read_version();
    if (v == -1 || v == 2) {
      for (std::size_t count = get_packet_length_v2() - buffer_.size(); count > 0 && begin != end; ++begin, --count) buffer_.push_back(*begin);
    } else if (v == data::version3 || v == data::version4) {
      // For v3/v4 the packet length is attacker-influenced via the wire
      // `buffer_length` field. Reject negative values up front (read_len
      // returns int32_t and was previously cast straight into size_t),
      // and refuse to grow past kMaxBufferBytes so a slow trickle cannot
      // tie up a megabyte per connection.
      // Until the fixed header has arrived the advertised length is not
      // known, so buffer the header first and then re-target on the length
      // it carries - a packet split inside its own header would otherwise
      // need an extra read before it could even be sized.
      while (begin != end) {
        std::size_t target = data::buffer_offset_v3;
        if (has_v3_header()) {
          const int32_t advertised = read_len();
          if (advertised < 0) {
            return boost::make_tuple(true, begin);
          }
          target = get_packet_length_v3();
          if (target > kMaxBufferBytes) {
            return boost::make_tuple(true, begin);
          }
        }
        if (buffer_.size() >= target) {
          break;
        }
        for (std::size_t count = target - buffer_.size(); count > 0 && begin != end; ++begin, --count) {
          if (buffer_.size() >= kMaxBufferBytes) {
            return boost::make_tuple(true, begin);
          }
          buffer_.push_back(*begin);
        }
      }
    }

    // Decide whether a whole packet is buffered. This used to short-circuit
    // with "complete" for any computed length below 1024 bytes, which is a
    // sanity check phrased as a completeness answer: it declared a packet
    // ready on a half-filled - or empty - buffer whenever the packet was
    // legitimately small (a split v3/v4 packet, or any v2 packet in a
    // deployment with `payload length` under 1012). The decoder then failed
    // on the truncated buffer. Report "not complete" while we are still
    // waiting, and only claim completeness for a malformed length so that
    // parse() can reject it with a proper error.
    v = read_version();
    std::size_t packet_length;
    if (v == data::version3 || v == data::version4) {
      if (!has_v3_header()) {
        return boost::make_tuple(false, begin);
      }
      packet_length = get_packet_length_v3();
      // A packet cannot be shorter than its own header.
      if (packet_length < data::buffer_offset_v3) {
        return boost::make_tuple(true, begin);
      }
    } else if (v == data::version2) {
      packet_length = get_packet_length_v2();
    } else if (v == -1) {
      // Not even the version has arrived yet.
      return boost::make_tuple(false, begin);
    } else {
      // An unknown version will never be filled by either arm above, so
      // hand it to parse() which rejects it with a proper error rather
      // than stalling until on_read gives up on the connection.
      return boost::make_tuple(true, begin);
    }
    if (packet_length > kMaxBufferBytes) {
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
  std::size_t size() const { return buffer_.size(); }
};
}  // namespace server
}  // namespace nrpe