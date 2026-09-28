// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <net/address_family.hpp>

namespace net {
// Internal child-process entry point. System name resolution cannot reliably be
// canceled, so deadline-sensitive callers run it before any agent initialization
// in a disposable process. Only a hostname and address family cross this boundary.
inline int resolve_host_cli(int argc, char *argv[]) {
  if (argc != 4) return 1;
  address_family family;
  if (!parse_address_family(argv[2], family)) return 1;
  boost::asio::io_context io;
  boost::asio::ip::udp::resolver resolver(io);
  boost::system::error_code error;
  const auto endpoints = resolve_for_family(resolver, family, argv[3], "0", error);
  if (error || endpoints.empty()) return 1;
  std::cout << endpoints.begin()->endpoint().address().to_string() << '\n';
  return 0;
}
}  // namespace net
