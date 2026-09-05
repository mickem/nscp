// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <net/nrpe/packet.hpp>
#include <net/socket/client.hpp>
#include <net/socket/socket_helpers.hpp>
#include <str/utf8.hpp>

using boost::asio::ip::tcp;

namespace nrpe {
namespace client {
class protocol : public boost::noncopyable {
 public:
  // traits
  typedef std::vector<char> read_buffer_type;
  typedef std::vector<char> write_buffer_type;
  typedef packet request_type;
  typedef std::list<packet> response_type;
  typedef socket_helpers::client::client_handler client_handler;
  static constexpr bool debug_trace = false;

 private:
  std::vector<char> buffer_;
  std::size_t payload_length_;
  std::shared_ptr<client_handler> handler_;
  response_type responses_;

  enum state { none, connected, has_request, sent_response, has_more, done };
  state current_state_;

  void set_state(const state new_state) { current_state_ = new_state; }

 public:
  explicit protocol(const std::shared_ptr<client_handler>& handler) : payload_length_(0), handler_(handler), current_state_(none) {}
  virtual ~protocol() {}

  void on_connect() { set_state(connected); }
  void prepare_request(request_type& packet) {
    set_state(has_request);
    payload_length_ = packet.get_payload_length();
    buffer_ = packet.get_buffer();
  }

  write_buffer_type& get_outbound() { return buffer_; }
  read_buffer_type& get_inbound() { return buffer_; }

  response_type get_timeout_response() {
    response_type ret;
    ret.push_back(nrpe::packet::unknown_response("Failed to read data"));
    return ret;
  }
  response_type get_response() { return responses_; }
  bool has_data() const { return current_state_ == has_request; }
  bool wants_data() const { return current_state_ == sent_response || current_state_ == has_more; }

  bool on_read(std::size_t bytes_transferred) {
    // Parse only what actually arrived. get_inbound() hands out the very
    // buffer the request was written from, and client::handle_read_request
    // calls on_read for a *partial* read too (a short response ends in eof),
    // so anything past bytes_transferred is still the tail of our own
    // request. Parsing buffer_.size() bytes fed that tail to the decoder,
    // whose CRC check then threw from inside an asio completion handler and
    // unwound through io_context::run_one() - a truncated response is an
    // error return, not an exception through the event loop.
    if (bytes_transferred == 0 || bytes_transferred > buffer_.size()) {
      responses_.push_back(nrpe::packet::unknown_response("Truncated response from NRPE server"));
      set_state(connected);
      return false;
    }
    try {
      const auto packet = nrpe::packet(&buffer_[0], bytes_transferred);
      if (packet.getType() == data::moreResponsePacket)
        set_state(has_more);
      else
        set_state(connected);
      responses_.push_back(packet);
    } catch (const std::exception &e) {
      responses_.push_back(nrpe::packet::unknown_response("Failed to read response: " + utf8::utf8_from_native(e.what())));
      set_state(connected);
      return false;
    }
    return true;
  }
  bool on_write(std::size_t) {
    set_state(sent_response);
    return true;
  }
  bool on_read_error(const boost::system::error_code&) {
    if (current_state_ == connected) {
      return true;
    }
    return false;
  }
};
}  // namespace client
}  // namespace nrpe