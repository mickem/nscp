// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collectd_sender.hpp"

#include <boost/asio.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <set>
#include <thread>

namespace {
using boost::asio::ip::udp;

// One socket aimed at the target endpoint. Multicast targets get one of these
// per local interface (bound to that interface's address so the datagram
// leaves through it); everything else gets a single socket whose source the
// routing table picks.
//
// The send is synchronous: a datagram either reaches the kernel or reports why
// it did not. The previous async send discarded the completion error code, so
// every failure - an unreachable target, a full socket buffer - looked exactly
// like a successful send, and a target that never received anything gave the
// operator nothing to go on.
class udp_sender {
 public:
  udp_sender(boost::asio::io_context &io_service, const udp::endpoint &source, const udp::endpoint &target)
      : target_(target), socket_(io_service, source) {
    // Binding the source address is not by itself what steers multicast egress
    // on every platform - IP_MULTICAST_IF is what selects the outgoing
    // interface - so set it too, or "through exactly these interfaces" holds
    // on some hosts and not others. Best effort: where the option is refused
    // the bound source and the routing table still apply. The IPv6 option
    // selects by interface index, which an address alone does not give us.
    if (target.address().is_v4() && source.address().is_v4()) {
      boost::system::error_code ec;
      socket_.set_option(boost::asio::ip::multicast::outbound_interface(source.address().to_v4()), ec);
    }
  }
  udp_sender(boost::asio::io_context &io_service, const udp::endpoint &target) : target_(target), socket_(io_service, target.protocol()) {}

  void send_data(const std::string &data, boost::system::error_code &ec) { socket_.send_to(boost::asio::buffer(data), target_, 0, ec); }

 private:
  udp::endpoint target_;
  udp::socket socket_;
};

bool is_multicast(const boost::asio::ip::address &address) {
  return (address.is_v4() && address.to_v4().is_multicast()) || (address.is_v6() && address.to_v6().is_multicast());
}

std::string trim_lower(const std::string &value) {
  std::string out;
  for (const char c : value) {
    if (std::isspace(static_cast<unsigned char>(c))) continue;
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

// Split a comma-separated interface list, dropping empty entries and
// surrounding whitespace.
std::list<std::string> split_list(const std::string &value) {
  std::list<std::string> out;
  std::string current;
  for (const char c : value) {
    if (c == ',') {
      if (!current.empty()) out.push_back(current);
      current.clear();
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c)) && current.empty()) continue;
    current.push_back(c);
  }
  if (!current.empty()) out.push_back(current);
  for (std::string &entry : out) {
    while (!entry.empty() && std::isspace(static_cast<unsigned char>(entry.back()))) entry.pop_back();
  }
  return out;
}
}  // namespace

collectd::sender_result collectd::send_datagrams(const sender_config &config, const std::list<std::string> &datagrams) {
  sender_result result;

  // An empty datagram is never put on the wire, so it is not a payload and
  // must not appear in the accounting either - counting it made a timeout
  // report more outstanding datagrams than there were.
  std::list<const std::string *> payloads;
  for (const std::string &datagram : datagrams) {
    if (!datagram.empty()) payloads.push_back(&datagram);
  }
  if (payloads.empty()) return result;
  const std::size_t total = payloads.size();
  // Payloads whose outcome has been recorded; the rest are still outstanding
  // if the send is cut short.
  std::size_t processed = 0;

  const std::string target_name = config.address + ":" + config.port;
  // Distinct failures only: the same error on every datagram of every cycle is
  // one log line, not hundreds.
  std::set<std::string> reported;
  const auto report = [&result, &reported](const std::string &message) {
    if (reported.insert(message).second) result.errors.push_back(message);
  };

  const auto started = std::chrono::steady_clock::now();
  const auto expired = [&config, &started]() {
    if (config.timeout_seconds == 0) return false;
    return std::chrono::steady_clock::now() - started >= std::chrono::seconds(config.timeout_seconds);
  };

  try {
    boost::asio::io_context io_service;

    // Resolve the configured address. This accepts host names as well as IP
    // literals: parsing only literals meant a target named by host name threw
    // on every metrics cycle - visible as a repeated log line and nothing
    // else - so the metrics silently never arrived.
    //
    // The lookup is asynchronous so it runs under the same budget as the send.
    // A blocking resolve() answers to the resolver's own timeout, which can be
    // tens of seconds on an unreachable DNS server - spent on the core metrics
    // thread, and outside the `timeout` this target documents.
    udp::resolver target_resolver(io_service);
    boost::system::error_code ec;
    udp::resolver::results_type targets;
    bool resolved = false;
    target_resolver.async_resolve(config.address, config.port, [&](const boost::system::error_code &e, const udp::resolver::results_type &r) {
      ec = e;
      targets = r;
      resolved = true;
    });
    if (config.timeout_seconds > 0) {
      io_service.run_for(std::chrono::seconds(config.timeout_seconds));
    } else {
      io_service.run();
    }
    if (!resolved) {
      target_resolver.cancel();
      io_service.restart();
      io_service.run();
      result.failed = total;
      report("Timed out after " + std::to_string(config.timeout_seconds) + "s resolving collectd target " + target_name + "; no metrics sent");
      return result;
    }
    io_service.restart();
    if (ec || targets.empty()) {
      result.failed = total;
      report("Failed to resolve collectd target " + target_name + ": " + (ec ? ec.message() : std::string("no addresses returned")));
      return result;
    }
    const udp::endpoint target = *targets.begin();

    // Build the set of sockets to send through. A unicast target - and a
    // multicast target left on "auto" - is one socket whose source the routing
    // table picks. "all" and an explicit list bind a socket per local
    // interface, so the datagram leaves through each of them.
    std::list<std::shared_ptr<udp_sender> > senders;
    if (is_multicast(target.address())) {
      const std::string mode = trim_lower(config.multicast_interfaces);
      if (mode == "all") {
        // Every local interface of the matching address family. Note that this
        // enumerates what the host name resolves to, which on a host whose name
        // maps to a loopback address (the 127.0.1.1 convention) is loopback and
        // nothing else - one reason "auto" is the default.
        udp::resolver resolver(io_service);
        boost::system::error_code local_ec;
        for (const auto &entry : resolver.resolve(boost::asio::ip::host_name(), "", local_ec)) {
          if (entry.endpoint().address().is_v4() != target.address().is_v4()) continue;
          // Skip an address that cannot be bound (an interface that went away,
          // a privileged or already-taken source) instead of letting it throw:
          // one bad entry must not stop the metrics leaving through the good
          // interfaces.
          try {
            senders.push_back(std::make_shared<udp_sender>(io_service, entry.endpoint(), target));
          } catch (const std::exception &e) {
            report("Cannot send to collectd target " + target_name + " through local address " + entry.endpoint().address().to_string() + ": " + e.what());
          }
        }
        if (senders.empty()) {
          report("No local interface of the target's address family could be enumerated for collectd target " + target_name +
                 " with 'multicast interface = all'; sending through the default route instead");
        }
      } else if (!mode.empty() && mode != "auto") {
        for (const std::string &entry : split_list(config.multicast_interfaces)) {
          boost::system::error_code address_ec;
          const boost::asio::ip::address local = boost::asio::ip::make_address(entry, address_ec);
          if (address_ec) {
            report("Ignoring 'multicast interface' entry '" + entry + "' for collectd target " + target_name +
                   ": not an IP address literal (host names are not accepted here)");
            continue;
          }
          if (local.is_v4() != target.address().is_v4()) {
            report("Ignoring 'multicast interface' entry '" + entry + "' for collectd target " + target_name +
                   ": wrong address family for the target group");
            continue;
          }
          try {
            senders.push_back(std::make_shared<udp_sender>(io_service, udp::endpoint(local, 0), target));
          } catch (const std::exception &e) {
            report("Cannot send to collectd target " + target_name + " through local address " + entry + ": " + e.what());
          }
        }
        if (senders.empty()) {
          result.failed = total;
          report("No usable 'multicast interface' entry for collectd target " + target_name + "; no metrics sent");
          return result;
        }
      }
    }
    if (senders.empty()) {
      // Unicast, or multicast on "auto": a default-bound socket for the
      // target's address family, routed like any other datagram.
      senders.push_back(std::make_shared<udp_sender>(io_service, target));
    }

    for (const std::string *payload : payloads) {
      if (expired()) {
        const std::size_t outstanding = total - processed;
        result.failed += outstanding;
        report("Timed out after " + std::to_string(config.timeout_seconds) + "s sending metrics to collectd target " + target_name + ": " +
               std::to_string(outstanding) + " datagram(s) not sent");
        break;
      }
      // One datagram is one unit of work whatever the interface count: it
      // counts as sent when every socket took it, and as failed otherwise.
      bool delivered = true;
      for (const std::shared_ptr<udp_sender> &sender : senders) {
        for (int attempt = 0;; attempt++) {
          boost::system::error_code send_ec;
          result.attempts++;
          sender->send_data(*payload, send_ec);
          if (!send_ec) break;
          if (attempt >= config.retries || expired()) {
            delivered = false;
            report("Failed to send metrics to collectd target " + target_name + ": " + send_ec.message());
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(retry_backoff_ms));
        }
      }
      processed++;
      if (delivered) {
        result.sent++;
      } else {
        result.failed++;
      }
    }
  } catch (const std::exception &e) {
    result.failed += total - processed;
    report("Failed to send metrics to collectd target " + target_name + ": " + e.what());
  }
  return result;
}
