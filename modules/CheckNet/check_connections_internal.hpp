// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
