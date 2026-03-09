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

#include <boost/asio.hpp>
#include <istream>
#include <net/icmp_header.hpp>
#include <net/ipv4_header.hpp>
#include <ostream>
#include <string>
#include <utility>

struct result_container {
  result_container() : num_send_(0), num_replies_(0), num_timeouts_(0), length_(0), sequence_number_(0), ttl_(0), time_(0) {}

  std::string destination_;
  std::string ip_;
  std::size_t num_send_;
  std::size_t num_replies_;
  std::size_t num_timeouts_;
  std::size_t length_;
  unsigned short sequence_number_;
  unsigned char ttl_;
  std::size_t time_;
};

class pinger {
 public:
  pinger(boost::asio::io_service& io_service, result_container& result, const char* destination, int timeout, unsigned short identifier, std::string payload)
      : resolver_(io_service),
        socket_(io_service, boost::asio::ip::icmp::v4()),
        timer_(io_service),
        sequence_number_(0),
        timeout_(timeout),
        result_(result),
        identifier_(identifier),
        payload_(std::move(payload))

  {
    boost::asio::ip::icmp::resolver::query query(boost::asio::ip::icmp::v4(), destination, "");
    destination_ = *resolver_.resolve(query);
    result.destination_ = destination;
    result.ip_ = destination_.address().to_string();
  }

  pinger(const pinger&) = delete;
  pinger& operator=(const pinger&) = delete;

  void ping() {
    start_send();
    start_receive();
  }

 private:
  void start_send() {
    std::string body(payload_);

    icmp_header echo_request;
    echo_request.type(icmp_header::echo_request);
    echo_request.code(0);
    echo_request.identifier(identifier_);
    echo_request.sequence_number(++sequence_number_);
    compute_checksum(echo_request, body.begin(), body.end());

    boost::asio::streambuf request_buffer;
    std::ostream os(&request_buffer);
    os << echo_request << body;

    result_.num_send_++;
    result_.sequence_number_ = sequence_number_;
    time_sent_ = boost::posix_time::microsec_clock::universal_time();
    socket_.send_to(request_buffer.data(), destination_);

    timer_.expires_at(time_sent_ + boost::posix_time::millisec(timeout_));
    timer_.async_wait([this](const boost::system::error_code& e) { this->handle_timeout(e); });
  }

  void handle_timeout(const boost::system::error_code& ec) {
    if (ec != boost::asio::error::operation_aborted) {
      result_.num_timeouts_++;
      socket_.close();
    }
  }

  void start_receive() {
    reply_buffer_.consume(reply_buffer_.size());
    socket_.async_receive(reply_buffer_.prepare(65536),
                          [this](const boost::system::error_code& ec, const std::size_t& length) { this->handle_receive(length, ec); });
  }

  void handle_receive(std::size_t length, const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
      return;
    }

    reply_buffer_.commit(length);

    std::istream is(&reply_buffer_);
    ipv4_header ipv4_hdr;
    icmp_header icmp_hdr;
    is >> ipv4_hdr >> icmp_hdr;

    if (is && icmp_hdr.type() == icmp_header::echo_reply && icmp_hdr.identifier() == identifier_ && icmp_hdr.sequence_number() == sequence_number_) {
      timer_.cancel();
      result_.num_replies_++;

      const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
      result_.length_ = length - ipv4_hdr.header_length();
      result_.ttl_ = ipv4_hdr.time_to_live();
      result_.time_ = (now - time_sent_).total_milliseconds();
    }
    // start_receive();
  }

  boost::asio::ip::icmp::resolver resolver_;
  boost::asio::ip::icmp::endpoint destination_;
  boost::asio::ip::icmp::socket socket_;
  boost::asio::deadline_timer timer_;
  unsigned short sequence_number_;
  boost::posix_time::ptime time_sent_;
  boost::asio::streambuf reply_buffer_;
  int timeout_;
  result_container& result_;
  unsigned short identifier_;
  std::string payload_;
};
