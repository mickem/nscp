// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <istream>
#include <net/address_family.hpp>
#include <net/icmp_header.hpp>
#include <net/ipv4_header.hpp>
#include <ostream>
#include <stdexcept>
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
  // `af` pins the IP version; with `any` the resolver picks and the socket is
  // opened in whichever family the destination resolved to.
  pinger(boost::asio::io_context& io_service, result_container& result, const char* destination, int timeout, unsigned short identifier, std::string payload,
         const net::address_family af = net::address_family::any)
      : resolver_(io_service),
        socket_(io_service),
        timer_(io_service),
        sequence_number_(0),
        timeout_(timeout),
        result_(result),
        identifier_(identifier),
        payload_(std::move(payload))

  {
    boost::system::error_code resolve_ec;
    const auto results = net::resolve_for_family(resolver_, af, destination, "", resolve_ec);
    if (resolve_ec) throw std::runtime_error(std::string("Failed to resolve ") + destination + ": " + resolve_ec.message());
    if (results.begin() == results.end())
      throw std::runtime_error(std::string("Failed to resolve ") + destination + " over " + net::to_string(af) + ": no address in the requested family");

    destination_ = results.begin()->endpoint();
    // ICMP and ICMPv6 are different protocols, not two modes of one: the socket
    // must be opened in the family the destination actually resolved to.
    is_v6_ = destination_.address().is_v6();
    socket_.open(destination_.protocol());
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
    echo_request.type(is_v6_ ? static_cast<unsigned char>(icmp_header::echo_request_v6) : static_cast<unsigned char>(icmp_header::echo_request));
    echo_request.code(0);
    echo_request.identifier(identifier_);
    echo_request.sequence_number(++sequence_number_);
    // The ICMPv6 checksum covers an IPv6 pseudo-header whose source address is
    // only chosen by the kernel at send time, so the kernel computes and
    // inserts it for raw IPPROTO_ICMPV6 sockets (RFC 3542 section 3.1) and we
    // leave the field at zero. IPv4 has no pseudo-header and no such kernel
    // support, so there we still compute it ourselves.
    if (!is_v6_) compute_checksum(echo_request, body.begin(), body.end());

    boost::asio::streambuf request_buffer;
    std::ostream os(&request_buffer);
    os << echo_request << body;

    result_.num_send_++;
    result_.sequence_number_ = sequence_number_;
    time_sent_ = std::chrono::steady_clock::now();
    socket_.send_to(request_buffer.data(), destination_);

    timer_.expires_at(time_sent_ + std::chrono::milliseconds(timeout_));
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
    // An IPv4 raw socket hands up the IP header with the payload; an ICMPv6 one
    // does not (the kernel strips it), so there the ICMP header is at offset 0.
    ipv4_header ipv4_hdr;
    icmp_header icmp_hdr;
    if (is_v6_)
      is >> icmp_hdr;
    else
      is >> ipv4_hdr >> icmp_hdr;

    const unsigned char expected_reply = is_v6_ ? static_cast<unsigned char>(icmp_header::echo_reply_v6) : static_cast<unsigned char>(icmp_header::echo_reply);
    if (is && icmp_hdr.type() == expected_reply && icmp_hdr.identifier() == identifier_ && icmp_hdr.sequence_number() == sequence_number_) {
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow it so it can't escape this
      // handler and propagate out of the caller's io_service.run(); the reply
      // is still recorded below.
      try {
        timer_.cancel();
      } catch (...) {
      }
      result_.num_replies_++;

      const auto now = std::chrono::steady_clock::now();
      // No IP header was consumed on v6, and the hop limit is only available
      // through IPV6_RECVHOPLIMIT ancillary data, which we do not request — so
      // ttl stays 0 there rather than reporting a fabricated value.
      result_.length_ = is_v6_ ? length : length - ipv4_hdr.header_length();
      result_.ttl_ = is_v6_ ? 0 : ipv4_hdr.time_to_live();
      result_.time_ = std::chrono::duration_cast<std::chrono::milliseconds>(now - time_sent_).count();
    }
    // start_receive();
  }

  boost::asio::ip::icmp::resolver resolver_;
  boost::asio::ip::icmp::endpoint destination_;
  boost::asio::ip::icmp::socket socket_;
  boost::asio::steady_timer timer_;
  unsigned short sequence_number_;
  std::chrono::steady_clock::time_point time_sent_;
  boost::asio::streambuf reply_buffer_;
  int timeout_;
  result_container& result_;
  unsigned short identifier_;
  std::string payload_;
  bool is_v6_ = false;
};
