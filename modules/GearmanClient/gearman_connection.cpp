// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_connection.hpp"

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <chrono>

namespace gearman {

namespace {
/**
 * How long `receive` waits before looking at the stop predicate again. It is
 * the only reason a wait is sliced at all, and it bounds how long a shutdown
 * takes to reach a worker asleep on a socket: nobody closes that socket from
 * another thread, because an Asio socket belongs to one.
 */
const unsigned int poll_slice_ms = 250;

/** Read granularity. A job is a few hundred bytes; this is one read for all of it. */
const std::size_t read_chunk_size = 8 * 1024;
}  // namespace

std::vector<server_address> parse_server_list(const std::string &spec) {
  std::vector<server_address> servers;
  std::string::size_type pos = 0;
  while (pos <= spec.size()) {
    const std::string::size_type comma = spec.find(',', pos);
    const std::string entry = boost::trim_copy(spec.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
    pos = comma == std::string::npos ? spec.size() + 1 : comma + 1;
    if (entry.empty()) continue;

    // An IPv6 literal is full of colons, so only a colon outside the brackets
    // separates the port: `[::1]:4730` is host `::1` port 4730, while `::1`
    // on its own is the whole host.
    if (entry[0] == '[') {
      const std::string::size_type close = entry.find(']');
      if (close == std::string::npos) throw connection_error("Unterminated IPv6 address in server list: " + entry);
      const std::string host = entry.substr(1, close - 1);
      const std::string rest = entry.substr(close + 1);
      if (rest.empty()) {
        servers.emplace_back(host, std::to_string(default_port));
      } else if (rest[0] == ':' && rest.size() > 1) {
        servers.emplace_back(host, rest.substr(1));
      } else {
        throw connection_error("Trailing junk after IPv6 address in server list: " + entry);
      }
      continue;
    }
    const std::string::size_type colon = entry.rfind(':');
    if (colon == std::string::npos || entry.find(':') != colon) {
      servers.emplace_back(entry, std::to_string(default_port));
    } else {
      servers.emplace_back(entry.substr(0, colon), entry.substr(colon + 1));
    }
  }
  return servers;
}

connection::connection() : socket_(io_), chunk_(read_chunk_size) {}

connection::~connection() {
  try {
    close();
  } catch (...) {
    // A destructor is not the place to report a socket that would not shut
    // down cleanly; the worker has already logged whatever led here.
  }
}

bool connection::is_open() const { return socket_.is_open(); }

bool connection::run_for(bool &flag, const unsigned int ms) {
  // One handler at a time rather than `run_for(ms)`: a connection can have a
  // read outstanding from a `receive` that timed out, and running until the
  // io_context goes idle would then keep a finished write waiting for that
  // read - a whole timeout of latency on every packet the worker sends.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (!flag) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) break;
    io_.restart();
    if (io_.run_one_for(deadline - now) == 0) break;
  }
  return flag;
}

void connection::drain() {
  boost::system::error_code ignored;
  socket_.cancel(ignored);
  io_.restart();
  io_.run();
}

void connection::close() {
  if (read_pending_) {
    // Let the outstanding read's handler run before the state it writes into
    // goes away with the connection.
    drain();
    read_pending_ = false;
    read_done_ = false;
  }
  boost::system::error_code ignored;
  if (socket_.is_open()) {
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
  }
  buffer_.clear();
}

void connection::connect(const server_address &server, const unsigned int timeout_seconds) {
  close();

  boost::asio::ip::tcp::resolver resolver(io_);
  boost::system::error_code resolve_error;
  const auto endpoints = resolver.resolve(server.host, server.port, boost::asio::ip::resolver_base::numeric_service, resolve_error);
  if (resolve_error) throw connection_error("Failed to resolve " + server.to_string() + ": " + resolve_error.message());

  bool done = false;
  boost::system::error_code connect_error;
  boost::asio::async_connect(socket_, endpoints, [&done, &connect_error](const boost::system::error_code &e, const boost::asio::ip::tcp::endpoint &) {
    connect_error = e;
    done = true;
  });
  if (!run_for(done, timeout_seconds * 1000)) {
    // The handler still holds references to `done` and `connect_error`, so it
    // has to run before this frame goes away.
    drain();
    throw connection_error("Timed out connecting to " + server.to_string() + " after " + std::to_string(timeout_seconds) + "s");
  }
  if (connect_error) throw connection_error("Failed to connect to " + server.to_string() + ": " + connect_error.message());

  // Nagle would sit on a 20 byte GRAB_JOB waiting for more to send, which is
  // a round trip of latency on every check this worker runs.
  boost::system::error_code ignored;
  socket_.set_option(boost::asio::ip::tcp::no_delay(true), ignored);
}

void connection::send(const packet_type type, const std::vector<std::string> &args, const unsigned int timeout_seconds) {
  if (!socket_.is_open()) throw connection_error("Cannot send " + to_string(type) + ": not connected");
  const std::string data = encode_packet(type, args);

  bool done = false;
  boost::system::error_code write_error;
  boost::asio::async_write(socket_, boost::asio::buffer(data), [&done, &write_error](const boost::system::error_code &e, const std::size_t) {
    write_error = e;
    done = true;
  });
  if (!run_for(done, timeout_seconds * 1000)) {
    drain();
    throw connection_error("Timed out sending " + to_string(type) + " after " + std::to_string(timeout_seconds) + "s");
  }
  if (write_error) throw connection_error("Failed to send " + to_string(type) + ": " + write_error.message());
}

connection::receive_result connection::receive(packet &out, const unsigned int timeout_seconds, const std::function<bool()> &should_stop) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
  for (;;) {
    // Whatever arrived on an earlier call may already be a whole packet, so
    // decode before reading: gearmand happily puts NOOP and JOB_ASSIGN in the
    // same segment.
    std::size_t consumed = 0;
    std::string error;
    switch (decode_packet(buffer_, out, consumed, error)) {
      case decode_result::ok:
        buffer_.erase(0, consumed);
        return receive_result::ok;
      case decode_result::invalid:
        throw connection_error("Not a gearman stream: " + error);
      case decode_result::incomplete:
        break;
    }

    if (read_done_) {
      read_done_ = false;
      read_pending_ = false;
      if (read_error_) {
        if (read_error_ == boost::asio::error::eof || read_error_ == boost::asio::error::connection_reset) return receive_result::closed;
        if (read_error_ != boost::asio::error::operation_aborted) throw connection_error("Failed to read: " + read_error_.message());
      } else if (read_bytes_ == 0) {
        return receive_result::closed;
      } else {
        buffer_.append(chunk_.data(), read_bytes_);
        continue;
      }
    }

    if (should_stop && should_stop()) return receive_result::timed_out;
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return receive_result::timed_out;

    if (!socket_.is_open()) return receive_result::closed;
    if (!read_pending_) {
      read_error_.clear();
      read_bytes_ = 0;
      socket_.async_read_some(boost::asio::buffer(chunk_), [this](const boost::system::error_code &e, const std::size_t bytes) {
        read_error_ = e;
        read_bytes_ = bytes;
        read_done_ = true;
      });
      read_pending_ = true;
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    run_for(read_done_, static_cast<unsigned int>(std::min<long long>(poll_slice_ms, remaining > 0 ? remaining : 1)));
  }
}

}  // namespace gearman
