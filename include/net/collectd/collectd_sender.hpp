// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <list>
#include <string>

namespace collectd {

// Where one target's datagrams go. Deliberately free of the plugin API so the
// send path can be driven against a real loopback socket from a unit test; the
// caller turns the reported problems into log lines.
struct sender_config {
  // Host name or IP literal, and the port as a string - both straight from the
  // target's settings.
  std::string address;
  std::string port;

  sender_config() {}
  sender_config(std::string address, std::string port) : address(std::move(address)), port(std::move(port)) {}
};

struct sender_result {
  // Datagrams that reached the socket, and datagrams that did not.
  std::size_t sent;
  std::size_t failed;
  // One line per problem, ready to log. Repeated identical failures are
  // collapsed so a broken target cannot flood the log every interval.
  std::list<std::string> errors;

  sender_result() : sent(0), failed(0) {}
};

// Send every datagram to the configured target.
//
// The address is resolved, so a target may name a host rather than an IP
// literal. A multicast target is sent through every local interface of the
// matching address family, as collectd's own network plugin does.
sender_result send_datagrams(const sender_config &config, const std::list<std::string> &datagrams);

}  // namespace collectd
