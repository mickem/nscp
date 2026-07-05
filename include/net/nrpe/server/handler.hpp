// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/noncopyable.hpp>
#include <list>
#include <net/nrpe/packet.hpp>

namespace nrpe {
namespace server {
class handler : boost::noncopyable {
 public:
  virtual ~handler() = default;
  // `peer_identity` is the trusted caller identity resolved by the
  // transport (today: client cert Subject DN extracted from the TLS
  // handshake, empty when no client cert was presented or SSL is off).
  // Forwarded to the core permission system as the principal.
  virtual std::list<packet> handle(packet packet, const std::string& peer_identity) = 0;
  virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
  virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
  virtual packet create_error(std::string msg) = 0;
  virtual unsigned int get_payload_length() = 0;
};
}  // namespace server
}  // namespace nrpe