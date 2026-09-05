// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collectd_sender.hpp"

#include <boost/asio.hpp>

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
      : target_(target), socket_(io_service, source) {}
  udp_sender(boost::asio::io_context &io_service, const udp::endpoint &target) : target_(target), socket_(io_service, target.protocol()) {}

  void send_data(const std::string &data, boost::system::error_code &ec) { socket_.send_to(boost::asio::buffer(data), target_, 0, ec); }

 private:
  udp::endpoint target_;
  udp::socket socket_;
};

bool is_multicast(const boost::asio::ip::address &address) {
  return (address.is_v4() && address.to_v4().is_multicast()) || (address.is_v6() && address.to_v6().is_multicast());
}
}  // namespace

collectd::sender_result collectd::send_datagrams(const sender_config &config, const std::list<std::string> &datagrams) {
  sender_result result;
  if (datagrams.empty()) return result;

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
    udp::resolver target_resolver(io_service);
    boost::system::error_code ec;
    const udp::resolver::results_type targets = target_resolver.resolve(config.address, config.port, ec);
    if (ec || targets.empty()) {
      result.failed = datagrams.size();
      report("Failed to resolve collectd target " + target_name + ": " + (ec ? ec.message() : std::string("no addresses returned")));
      return result;
    }
    const udp::endpoint target = *targets.begin();

    // Build the set of sockets to send through. For unicast that is a single
    // OS-routed socket; for multicast we send out every local interface of the
    // matching address family.
    std::list<std::shared_ptr<udp_sender> > senders;
    if (is_multicast(target.address())) {
      udp::resolver resolver(io_service);
      boost::system::error_code local_ec;
      for (const auto &entry : resolver.resolve(boost::asio::ip::host_name(), "", local_ec)) {
        if (entry.endpoint().address().is_v4() == target.address().is_v4()) {
          senders.push_back(std::make_shared<udp_sender>(io_service, entry.endpoint(), target));
        }
      }
    }
    if (senders.empty()) {
      // Unicast, or multicast with no enumerable matching interface: fall back
      // to a default-bound socket for the target's address family.
      senders.push_back(std::make_shared<udp_sender>(io_service, target));
    }

    std::size_t remaining = datagrams.size();
    for (const std::string &datagram : datagrams) {
      remaining--;
      if (datagram.empty()) continue;  // never put an empty datagram on the wire
      if (expired()) {
        result.failed += remaining + 1;
        report("Timed out after " + std::to_string(config.timeout_seconds) + "s sending metrics to collectd target " + target_name + ": " +
               std::to_string(remaining + 1) + " datagram(s) not sent");
        break;
      }
      for (const std::shared_ptr<udp_sender> &sender : senders) {
        for (int attempt = 0;; attempt++) {
          boost::system::error_code send_ec;
          result.attempts++;
          sender->send_data(datagram, send_ec);
          if (!send_ec) {
            result.sent++;
            break;
          }
          if (attempt >= config.retries || expired()) {
            result.failed++;
            report("Failed to send metrics to collectd target " + target_name + ": " + send_ec.message());
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(retry_backoff_ms));
        }
      }
    }
  } catch (const std::exception &e) {
    result.failed = datagrams.size() - result.sent;
    report("Failed to send metrics to collectd target " + target_name + ": " + e.what());
  }
  return result;
}
