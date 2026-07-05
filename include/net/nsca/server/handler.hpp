// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <net/nsca/nsca_packet.hpp>

namespace nsca {
namespace server {
class handler : boost::noncopyable {
 public:
  virtual void handle(packet packet) = 0;
  virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
  virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
  virtual unsigned int get_payload_length() = 0;
  virtual int get_encryption() = 0;
  virtual std::string get_password() = 0;
  // Reference timezone for wire timestamps. Default "utc" matches the
  // protocol specification. Subclasses may return "local" or a POSIX TZ
  // string to interoperate with peers that emit local-clock-as-Unix-time
  // stamps (a legacy bug that some agents still ship with). Note that
  // returning an empty string would resolve to "local" via nscp_time, so
  // the explicit "utc" default matters.
  virtual std::string get_timezone() { return "utc"; }
};
}  // namespace server
}  // namespace nsca