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
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <cstddef>

namespace nsca_ng {

/// Escape special characters in a check-result field value.
///
/// Nagios external commands separate fields with `;`, so any `;` inside a
/// field value would corrupt the on-wire framing — we escape it as `\;`.
/// `\` is escaped to `\\` so the receiving side can recover the original
/// value, and `\n` is escaped to the two-character sequence `\n` so a single
/// command stays on a single line. (B1 fix: prior versions only escaped
/// backslash and newline, silently corrupting any check output containing
/// a semicolon — e.g. `OK: 3 services; all up`.)
std::string escape_field(const std::string &value);

/// Build a Nagios external command string for one check result.
/// When @p service is empty the result is treated as a host check and the
/// command is "PROCESS_HOST_CHECK_RESULT"; otherwise it is a service check
/// using "PROCESS_SERVICE_CHECK_RESULT".
/// @param host      Target host name.
/// @param service   Service description, or empty for host checks.
/// @param code      Nagios return code (0=OK, 1=WARN, 2=CRIT, 3=UNKN).
/// @param output    Plugin output string.
/// @param timestamp Unix timestamp to embed in the command bracket.
/// @return          The formatted external-command string (no trailing newline).
std::string build_check_result_command(const std::string &host, const std::string &service, int code, const std::string &output, long timestamp);

/// Build the NSCA-NG MOIN handshake line: "MOIN 1 <session_id>".
std::string build_moin_request(const std::string &session_id);

/// Build the NSCA-NG PUSH command line: "PUSH <length>".
std::string build_push_request(std::size_t length);

/// Possible server-response keywords.
struct server_response {
  enum class type { okay, fail, bail, moin, unknown };
  type kind;
  std::string message;  ///< For FAIL/BAIL: reason string; for MOIN: version; for OKAY: empty.
  bool ok() const { return kind == type::okay || kind == type::moin; }
};

/// Parse a single line received from the server.
server_response parse_server_response(const std::string &line);

}  // namespace nsca_ng
