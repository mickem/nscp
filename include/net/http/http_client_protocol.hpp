// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <net/http/http_packet.hpp>
#include <net/socket/client.hpp>

using boost::asio::ip::tcp;

namespace http {
namespace client {

class protocol : public boost::noncopyable {
 public:
  // traits
  typedef std::vector<char> read_buffer_type;
  typedef std::vector<char> write_buffer_type;
  typedef request request_type;
  typedef response response_type;
  typedef socket_helpers::client::client_handler client_handler;
  static const bool debug_trace = false;

 private:
  std::vector<char> buffer_;
  std::shared_ptr<client_handler> handler_;
  request_type packet_;
  std::vector<char> responseData_;

  enum state { none, connected, has_data_to_send, wants_data_to_read, done };
  state current_state_;

  void set_state(state new_state) { current_state_ = new_state; }

 public:
  protocol(std::shared_ptr<client_handler> handler) : handler_(handler), current_state_(none) {}
  virtual ~protocol() = default;

  void on_connect() { set_state(connected); }
  void prepare_request(request_type& packet) {
    packet_ = packet;
    prepare_to_send();
  }

  write_buffer_type& get_outbound() { return buffer_; }
  read_buffer_type& get_inbound() { return buffer_; }

  static response_type get_timeout_response() { return response::create_timeout("Failed to read data"); }
  response_type get_response() const { return response_type(responseData_); }
  bool has_data() const { return current_state_ == has_data_to_send; }
  bool wants_data() const { return current_state_ == wants_data_to_read; }

  bool on_read(std::size_t) {
    if (current_state_ == wants_data_to_read) {
      responseData_.insert(responseData_.end(), buffer_.begin(), buffer_.end());
      return true;
    }
    set_state(done);
    return true;
  }
  bool prepare_to_send() {
    set_state(has_data_to_send);
    buffer_ = packet_.get_packet();
    return true;
  }
  bool on_write(std::size_t) {
    set_state(wants_data_to_read);
    return false;
  }
  bool on_read_error(const boost::system::error_code& _e) {
    if (current_state_ == wants_data_to_read) {
      set_state(done);
      return true;
    }
    return false;
  }
};
}  // namespace client
}  // namespace http
