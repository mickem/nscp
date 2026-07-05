// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <net/nrpe/packet.hpp>
#include <net/socket/client.hpp>
#include <net/socket/socket_helpers.hpp>

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

  bool on_read(std::size_t) {
    const auto packet = nrpe::packet(&buffer_[0], static_cast<unsigned int>(buffer_.size()));
    if (packet.getType() == data::moreResponsePacket)
      set_state(has_more);
    else
      set_state(connected);
    responses_.push_back(packet);
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