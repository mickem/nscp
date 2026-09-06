// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <list>
#include <string>

namespace collectd {

// Milliseconds waited between two attempts at the same datagram. A send that
// failed locally usually failed because a buffer was momentarily full, so an
// immediate retry would fail the same way; the wait is short enough that the
// whole retry budget stays well inside one metrics interval.
static const unsigned int retry_backoff_ms = 20;

// Where one target's datagrams go. Deliberately free of the plugin API so the
// send path can be driven against a real loopback socket from a unit test; the
// caller turns the reported problems into log lines.
struct sender_config {
  // Host name or IP literal, and the port as a string - both straight from the
  // target's settings.
  std::string address;
  std::string port;

  // Extra attempts after a failed send (the target's `retries` setting). A UDP
  // send that fails locally never left the machine, so re-attempting it cannot
  // duplicate a datagram the receiver already has - which is why this counts
  // failed sends only and never re-sends a delivered one.
  int retries;

  // Wall-clock budget for the whole send, in seconds (the target's `timeout`
  // setting); 0 means no limit. It covers name resolution as well as the retry
  // loop, so neither an unreachable target nor an unresponsive DNS server can
  // hold the metrics thread past it.
  unsigned int timeout_seconds;

  // Which local interfaces a multicast target's datagrams leave through (the
  // target's `multicast interface` setting): "auto" or empty for the one the
  // routing table picks, "all" for every local interface of the target's
  // address family, or a comma-separated list of local IP addresses. Ignored
  // for a unicast target, which the routing table has always decided.
  std::string multicast_interfaces;

  sender_config() : retries(0), timeout_seconds(0) {}
  sender_config(std::string address, std::string port, int retries = 0, unsigned int timeout_seconds = 0, std::string multicast_interfaces = "")
      : address(std::move(address)),
        port(std::move(port)),
        retries(retries),
        timeout_seconds(timeout_seconds),
        multicast_interfaces(std::move(multicast_interfaces)) {}
};

struct sender_result {
  // Datagrams delivered to every socket the target uses, and datagrams that
  // were not - including the ones a spent timeout left unsent. Both count
  // payloads, so together they account for every non-empty datagram passed in,
  // whatever the number of interfaces behind a multicast target.
  std::size_t sent;
  std::size_t failed;
  // Send calls made, including retries and one per interface: `attempts` above
  // `sent` means the retry budget was being spent, or the target fans out over
  // several interfaces.
  std::size_t attempts;
  // One line per problem, ready to log. Repeated identical failures are
  // collapsed so a broken target cannot flood the log every interval.
  std::list<std::string> errors;

  sender_result() : sent(0), failed(0), attempts(0) {}
};

// Send every datagram to the configured target.
//
// The address is resolved, so a target may name a host rather than an IP
// literal; the datagrams go to the first address the resolver returns - the
// one any other client would use for that name - so a name with both an A and
// an AAAA record follows the host's own address preference. Name a literal on
// the target when the collectd daemon listens on one family only.
//
// A multicast target goes out through the interface(s) named by
// `multicast_interfaces`.
sender_result send_datagrams(const sender_config &config, const std::list<std::string> &datagrams);

}  // namespace collectd
