// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio.hpp>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "gearman_protocol.hpp"

/**
 * One socket to a gearmand job server.
 *
 * The worker loop is written as straight-line code - connect, register, ask
 * for a job, run it, answer - so this wraps Asio in blocking calls rather
 * than callbacks. Every one of them carries a deadline: a peer that accepts
 * the connection and then says nothing must not wedge a worker thread, which
 * is how an agent silently stops answering checks until the service is
 * restarted. (SO_RCVTIMEO does not achieve this - Asio reads the resulting
 * EAGAIN as "would block" and then waits on the descriptor with no deadline
 * of its own, the same trap documented on `run_with_deadline` in
 * `net/http/client.hpp`. The operation has to be issued asynchronously and
 * the io_context run with a deadline.)
 *
 * A connection is owned and used by exactly one thread. Nothing here is safe
 * to call from a second one, and nothing needs to be: `receive` polls a
 * caller-supplied predicate while it waits, so a shutdown is noticed without
 * anyone reaching into the socket from outside.
 */
namespace gearman {

/** Thrown when the connection fails or the peer is not speaking gearman. */
class connection_error : public std::runtime_error {
 public:
  explicit connection_error(const std::string &what) : std::runtime_error(what) {}
};

/**
 * Thrown when a submission reached the wire but its acknowledgement did not
 * come back.
 *
 * The distinction is the whole point of the type: a payload that never left
 * can be sent again on a fresh connection, and one that may already be on the
 * queue cannot - a result the core files twice is a worse failure than one it
 * never sees, because nothing downstream can tell the duplicate from a real
 * second check.
 */
class unconfirmed_submission : public connection_error {
 public:
  explicit unconfirmed_submission(const std::string &what) : connection_error(what) {}
};

/** One entry of the `server` setting: `host` or `host:port`. */
struct server_address {
  std::string host;
  /** Kept as text because that is what the resolver takes. */
  std::string port;

  server_address() : port(std::to_string(default_port)) {}
  server_address(std::string host_, std::string port_) : host(std::move(host_)), port(std::move(port_)) {}

  std::string to_string() const { return host + ":" + port; }
};

/**
 * Split a comma separated `host[:port]` list. An entry without a port gets
 * 4730; blank entries are dropped, so a trailing comma is not an error. An
 * IPv6 literal is written in brackets (`[::1]:4730`), as everywhere else in
 * the agent.
 */
std::vector<server_address> parse_server_list(const std::string &spec);

class connection {
 public:
  enum class receive_result {
    /** A packet was decoded into `out`. */
    ok,
    /** The deadline passed, or the stop predicate asked us to give up. */
    timed_out,
    /** The peer closed the connection. Reconnect. */
    closed
  };

  connection();
  ~connection();

  connection(const connection &) = delete;
  connection &operator=(const connection &) = delete;

  /** Resolve and connect within `timeout_seconds`. Throws `connection_error`. */
  void connect(const server_address &server, unsigned int timeout_seconds);
  bool is_open() const;
  /** Idempotent, and safe to call on a connection that never connected. */
  void close();

  /** Write one request packet within `timeout_seconds`. Throws `connection_error`. */
  void send(packet_type type, const std::vector<std::string> &args = std::vector<std::string>(), unsigned int timeout_seconds = 30);

  /**
   * Read one packet, waiting at most `timeout_seconds`.
   *
   * `should_stop` is polled about once a second while waiting and makes the
   * call return `timed_out` early; this is how a worker notices a shutdown
   * during the thirty second sleep after `PRE_SLEEP` without anyone closing
   * its socket from another thread. Throws `connection_error` when the
   * stream is not gearman or the read fails.
   */
  receive_result receive(packet &out, unsigned int timeout_seconds, const std::function<bool()> &should_stop = std::function<bool()>());

  /**
   * Put one payload on `queue` as a background job and wait for its
   * `JOB_CREATED`.
   *
   * The unique id is deliberately empty, and that is not an omission.
   * gearmand coalesces a submission onto an existing job with the same
   * function and unique id - for a background job as much as a foreground one
   * - answering with the older job's handle and dropping the payload just
   * sent. On a result queue that is silent data loss: while the core is
   * behind, every result for a host whose earlier result is still queued is
   * thrown away, and the submitter is told the submission succeeded.
   * mod_gearman's own result senders pass no unique id for exactly this
   * reason; its `use_uniq_jobs` option is about check *jobs*, where
   * collapsing a duplicate check is the wanted behaviour.
   *
   * Waiting for the acknowledgement is what separates "gearmand has it" from
   * "it is still in a socket buffer", which a background job does not
   * otherwise tell you.
   *
   * Throws `connection_error` while the payload has not left this host, and
   * `unconfirmed_submission` once it has; see that type for why a caller must
   * treat the two differently.
   */
  void submit_background(const std::string &queue, const std::string &payload, unsigned int timeout_seconds,
                         const std::function<bool()> &should_stop = std::function<bool()>());

 private:
  /** Run the io_context for at most `ms`, then report whether `flag` was set. */
  bool run_for(bool &flag, unsigned int ms);
  /** Cancel whatever is outstanding and let its handler run, so no handler outlives its state. */
  void drain();

  boost::asio::io_context io_;
  boost::asio::ip::tcp::socket socket_;

  /** Where `connect` last went, so a failure can name the server it was for. */
  server_address server_;

  /** Bytes read but not yet decoded into a packet. */
  std::string buffer_;

  /**
   * The read in flight, if any. It is a member rather than a local because a
   * `receive` that times out leaves the operation outstanding: its handler
   * would otherwise write into a destroyed stack frame when the data finally
   * arrives, and cancelling on every timeout would throw away bytes the peer
   * has already sent.
   */
  std::vector<char> chunk_;
  bool read_pending_ = false;
  bool read_done_ = false;
  boost::system::error_code read_error_;
  std::size_t read_bytes_ = 0;
};

}  // namespace gearman
