// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collectd_sender.hpp"

#include <boost/asio.hpp>

#include <memory>

namespace {
using boost::asio::ip::udp;

// One socket aimed at the target endpoint. Multicast targets get one of these
// per local interface (bound to that interface's address so the datagram
// leaves through it); everything else gets a single socket whose source the
// routing table picks.
class udp_sender {
 public:
  udp_sender(boost::asio::io_context &io_service, const udp::endpoint &source, const udp::endpoint &target)
      : target_(target), socket_(io_service, source) {}
  udp_sender(boost::asio::io_context &io_service, const udp::endpoint &target) : target_(target), socket_(io_service, target.protocol()) {}

  // Queue one datagram. The payload is owned by a shared_ptr captured in the
  // completion handler so it stays alive until the async send finishes - this
  // lets us queue several packets before a single io_context.run() drains them.
  void send_data(const std::string &data) {
    auto payload = std::make_shared<std::string>(data);
    socket_.async_send_to(boost::asio::buffer(*payload), target_, [payload](const boost::system::error_code &, std::size_t) {});
  }

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
      result.errors.push_back("Failed to resolve collectd target " + config.address + ":" + config.port + ": " +
                              (ec ? ec.message() : std::string("no addresses returned")));
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

    for (const std::string &datagram : datagrams) {
      if (datagram.empty()) continue;  // never put an empty datagram on the wire
      for (const std::shared_ptr<udp_sender> &sender : senders) {
        sender->send_data(datagram);
      }
      result.sent++;
    }
    io_service.run();
  } catch (const std::exception &e) {
    result.failed = datagrams.size() - result.sent;
    result.errors.push_back("Failed to send metrics to collectd target " + config.address + ":" + config.port + ": " + e.what());
  }
  return result;
}
