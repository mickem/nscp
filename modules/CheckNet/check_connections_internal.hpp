/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Small pure helpers used by check_connections; broken out so they can be
 * unit tested without touching the system.
 */

#pragma once

namespace check_net {
namespace check_connections_internal {

// Mapping from /proc/net/{tcp,tcp6} hex state to canonical state name.
// See linux/net/tcp_states.h.
inline const char *linux_tcp_state(unsigned int s) {
  switch (s) {
    case 0x01:
      return "ESTABLISHED";
    case 0x02:
      return "SYN_SENT";
    case 0x03:
      return "SYN_RECV";
    case 0x04:
      return "FIN_WAIT1";
    case 0x05:
      return "FIN_WAIT2";
    case 0x06:
      return "TIME_WAIT";
    case 0x07:
      return "CLOSE";
    case 0x08:
      return "CLOSE_WAIT";
    case 0x09:
      return "LAST_ACK";
    case 0x0A:
      return "LISTEN";
    case 0x0B:
      return "CLOSING";
    default:
      return "UNKNOWN";
  }
}

}  // namespace check_connections_internal
}  // namespace check_net
