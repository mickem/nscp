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

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <net/socket/server.hpp>
#include <net/socket/socket_helpers.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>

#include "handler.hpp"
#include "parser.hpp"

namespace nrpe {
using boost::asio::ip::tcp;

//
// Connection states:
// on_accept
// on_connect	-> connected	wants_data = true
// on_read		-> has_more		has_data = true
// on_write		-> has_more		has_data = true
// on_write		-> last_packet	has_data = true
// on_write		-> done

static constexpr int socket_bufer_size = 8096;
struct read_protocol : boost::noncopyable {
  static constexpr bool debug_trace = false;

  typedef std::vector<char> outbound_buffer_type;
  typedef server::handler *handler_type;
  typedef boost::array<char, socket_bufer_size>::iterator iterator_type;

  enum state { none, connected, has_more, last_packet, done };

  socket_helpers::connection_info info_;
  handler_type handler_;
  server::parser parser_;
  state current_state_;
  outbound_buffer_type data_;
  std::list<packet> responses_;
  int version_;

  static boost::shared_ptr<read_protocol> create(socket_helpers::connection_info info, handler_type handler) {
    return boost::make_shared<read_protocol>(info, handler);
  }

  read_protocol(const socket_helpers::connection_info &info, const handler_type handler)
      : info_(info), handler_(handler), parser_(handler->get_payload_length()), current_state_(none), version_(data::version2) {}

  void set_state(const state new_state) { current_state_ = new_state; }

  bool on_accept(const tcp::socket &socket, const int count) {
    std::list<std::string> errors;
    parser_.reset();
    const std::string s = socket.remote_endpoint().address().to_string();
    if (info_.allowed_hosts.is_allowed(socket.remote_endpoint().address(), errors)) {
      log_debug(__FILE__, __LINE__, "Accepting connection from: " + s + ", count=" + str::xtos(count));
      return true;
    }
    for (const std::string &e : errors) {
      log_error(__FILE__, __LINE__, e);
    }
    log_error(__FILE__, __LINE__, "Rejected connection from: " + s);
    return false;
  }

  bool on_connect() {
    set_state(connected);
    return true;
  }

  bool wants_data() const { return current_state_ == connected; }
  bool has_data() const { return current_state_ == has_more || current_state_ == last_packet; }

  bool on_read(char *begin, char *end) {
    while (begin != end) {
      bool result;
      const iterator_type old_begin = begin;
      boost::tie(result, begin) = parser_.digest(begin, end);
      if (result) {
        try {
          const packet request = parser_.parse();
          version_ = request.getVersion();
          responses_ = handler_->handle(request);
        } catch (const std::exception &e) {
          responses_.push_back(handler_->create_error("Exception processing request: " + utf8::utf8_from_native(e.what())));
        } catch (...) {
          responses_.push_back(handler_->create_error("Exception processing request"));
        }
        queue_next();
        return true;
      }
      if (begin == old_begin) {
        log_error(__FILE__, __LINE__, "Digester failed to parse NRPE data after " + str::xtos(parser_.size()) + " bytes, giving up.");
        return false;
      }
    }
    return true;
  }
  bool has_more_response() const { return !responses_.empty(); }
  void queue_next() {
    try {
      data_ = responses_.front().get_buffer();
      responses_.pop_front();
      if (has_more_response())
        set_state(has_more);
      else
        set_state(last_packet);
    } catch (const std::exception &e) {
      log_debug(__FILE__, __LINE__, "Failed to create return package: " + utf8::utf8_from_native(e.what()));
    }
  }
  void on_write() {
    if (current_state_ == last_packet)
      set_state(done);
    else
      queue_next();
  }
  std::vector<char> get_outbound() const { return data_; }

  socket_helpers::connection_info get_info() const { return info_; }

  void log_debug(const std::string &file, const int line, const std::string &msg) const { handler_->log_debug("nrpe", file, line, msg); }
  void log_error(const std::string &file, const int line, const std::string &msg) const { handler_->log_error("nrpe", file, line, msg); }
};

namespace server {
typedef socket_helpers::server::server<read_protocol, socket_bufer_size> server;
}
}  // namespace nrpe