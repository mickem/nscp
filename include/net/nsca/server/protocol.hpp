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
#include <cstdint>
#include <ctime>
#include <net/socket/server.hpp>
#include <net/socket/socket_helpers.hpp>
#include <str/xtos.hpp>

#include "handler.hpp"
#include "parser.hpp"

namespace nsca {
using boost::asio::ip::tcp;

static constexpr int socket_bufer_size = 8096;
struct read_protocol : boost::noncopyable {
  static constexpr bool debug_trace = false;

  typedef std::string outbound_buffer_type;
  typedef server::handler *handler_type;
  typedef boost::array<char, socket_bufer_size>::iterator iterator_type;

  enum state { none, connected, sent_iv, done };

  socket_helpers::connection_info info_;
  handler_type handler_;
  server::parser parser_;
  state current_state_;

  std::string data_;
  nscp::encryption::engine encryption_instance_;

  static boost::shared_ptr<read_protocol> create(const socket_helpers::connection_info &info, handler_type handler) {
    return boost::make_shared<read_protocol>(info, handler);
  }

  read_protocol(const socket_helpers::connection_info &info, const handler_type handler)
      : info_(info), handler_(handler), parser_(handler->get_payload_length()), current_state_(none) {}

  void set_state(const state new_state) { current_state_ = new_state; }

  bool on_accept(const tcp::socket &socket, int count) {
    std::list<std::string> errors;
    parser_.reset();
    const std::string s = socket.remote_endpoint().address().to_string();
    if (info_.allowed_hosts.is_allowed(socket.remote_endpoint().address(), errors)) {
      log_debug(__FILE__, __LINE__, "Accepting connection from: " + s);
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
    std::vector<boost::asio::const_buffer> buffers;

    const std::string iv = nscp::encryption::engine::generate_transmitted_iv();
    encryption_instance_.encrypt_init(handler_->get_password(), handler_->get_encryption(), iv);

    const iv_packet packet(iv, boost::posix_time::second_clock::local_time());
    data_ = packet.get_buffer();
    return true;
  }

  bool wants_data() const { return current_state_ == sent_iv; }
  bool has_data() const { return current_state_ == connected; }

  bool on_write() {
    set_state(sent_iv);
    return true;
  }

  // Maximum permitted clock skew between the timestamp embedded in the NSCA
  // packet and our local clock. A captured packet replayed outside this
  // window is rejected even when the attacker holds the password (which
  // would otherwise let them re-encrypt and resubmit any historical
  // submission thanks to the protocol's byte-CFB malleability). Five
  // minutes covers normal NTP drift; legitimate clients submitting fresh
  // results sit well inside it.
  static constexpr int kMaxClockSkewSeconds = 5 * 60;

  bool on_read(char *begin, char *end) {
    while (begin != end) {
      bool result;
      const iterator_type old_begin = begin;
      boost::tie(result, begin) = parser_.digest(begin, end);
      if (begin == old_begin) {
        log_error(__FILE__, __LINE__, "Digester failed to parse chunk, giving up.");
        return false;
      }
      if (result) {
        set_state(done);
        packet response;
        try {
          parser_.decrypt(encryption_instance_);
          const packet request = parser_.parse();
          // Replay window check. The packet carries its own creation
          // timestamp; reject anything more than five minutes off the local
          // clock in either direction. Without this guard, an attacker who
          // captured one valid submission and held the symmetric key (or
          // re-derived ciphertext via byte-CFB malleability) could resubmit
          // arbitrary historical results.
          const std::time_t now_t = std::time(nullptr);
          const std::int64_t request_t = request.time;
          const std::int64_t skew = static_cast<std::int64_t>(now_t) - request_t;
          if (skew > kMaxClockSkewSeconds || skew < -kMaxClockSkewSeconds) {
            log_error(__FILE__, __LINE__,
                      "Rejecting NSCA submission with timestamp out of range (skew=" + str::xtos(skew) + "s); check NTP on the sender or treat as replay.");
          } else {
            handler_->handle(request);
          }
        } catch (const std::exception &e) {
          log_error(__FILE__, __LINE__, std::string("Exception processing request: ") + e.what());
          log_debug(__FILE__, __LINE__, "Using: encryption = " + nscp::encryption::helpers::encryption_to_string(handler_->get_encryption()));
        } catch (...) {
          log_error(__FILE__, __LINE__, "Exception processing request");
        }
        return false;
      }
    }
    return true;
  }
  std::string get_outbound() const { return data_; }

  socket_helpers::connection_info get_info() const { return info_; }

  void log_debug(const std::string &file, const int line, const std::string &msg) const { handler_->log_debug("nsca", file, line, msg); }
  void log_error(const std::string &file, const int line, const std::string &msg) const { handler_->log_error("nsca", file, line, msg); }
};

namespace server {
typedef socket_helpers::server::server<read_protocol, socket_bufer_size> server;
}
}  // namespace nsca