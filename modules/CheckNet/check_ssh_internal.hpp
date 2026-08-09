// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <string>

namespace check_net {
namespace check_ssh_internal {

// The pieces of an SSH identification string (RFC 4253 section 4.2):
//
//   SSH-<protoversion>-<softwareversion>[ <comments>]
//
// e.g. "SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5" yields
//   protocol         = "2.0"   (major 2, minor 0)
//   version          = "OpenSSH_9.6p1"
//   software         = "OpenSSH"
//   software_version = "9.6p1"
//   comments         = "Ubuntu-3ubuntu13.5"
struct ssh_banner {
  std::string banner;
  std::string protocol;
  long long protocol_major = 0;
  long long protocol_minor = 0;
  std::string version;
  std::string software;
  std::string software_version;
  std::string comments;
};

namespace detail {

// Parse a leading run of digits. Returns the value and advances `pos` past it;
// returns false when there is no digit at `pos` (so a non-numeric protocol
// version leaves the numeric keywords at 0 rather than guessing).
inline bool parse_uint(const std::string &s, std::size_t &pos, long long &out) {
  const std::size_t start = pos;
  long long value = 0;
  while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
    // Saturate rather than overflow on absurd input; the exact value of a
    // 19-digit protocol version does not matter, only that it is huge.
    if (value < 1000000000LL) value = value * 10 + (s[pos] - '0');
    ++pos;
  }
  if (pos == start) return false;
  out = value;
  return true;
}

// Split "<software>_<version>" on the LAST underscore that is followed by a
// digit. That handles the plain "OpenSSH_9.6p1" and "dropbear_2022.83" forms as
// well as the multi-word "OpenSSH_for_Windows_9.5" and "Sun_SSH_1.1" ones,
// which a first-underscore split would cut in the wrong place. Software with no
// underscore at all (e.g. "Go") keeps the whole string as the name.
inline void split_software(const std::string &version, std::string &software, std::string &software_version) {
  software = version;
  software_version.clear();
  for (std::size_t i = version.size(); i-- > 0;) {
    if (version[i] != '_') continue;
    if (i + 1 < version.size() && std::isdigit(static_cast<unsigned char>(version[i + 1]))) {
      software = version.substr(0, i);
      software_version = version.substr(i + 1);
      return;
    }
  }
}

}  // namespace detail

// Extract the SSH identification string from whatever the server sent and split
// it into its parts. A server may emit other lines before the identification
// string (RFC 4253 allows it), so the first line starting with "SSH-" wins.
// Returns false — leaving `out` untouched — when no usable identification
// string is present.
inline bool parse_ssh_banner(const std::string &raw, ssh_banner &out) {
  std::size_t line_start = 0;
  while (line_start <= raw.size()) {
    std::size_t line_end = raw.find('\n', line_start);
    if (line_end == std::string::npos) line_end = raw.size();

    std::string line = raw.substr(line_start, line_end - line_start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    line_start = line_end + 1;

    if (line.compare(0, 4, "SSH-") != 0) {
      if (line_end == raw.size()) break;
      continue;
    }

    // "<protoversion>-<softwareversion>[ <comments>]"
    const std::string rest = line.substr(4);
    const std::size_t dash = rest.find('-');
    if (dash == std::string::npos) return false;

    const std::string protocol = rest.substr(0, dash);
    std::string version = rest.substr(dash + 1);
    if (protocol.empty()) return false;

    std::string comments;
    const std::size_t space = version.find(' ');
    if (space != std::string::npos) {
      comments = version.substr(space + 1);
      version = version.substr(0, space);
    }
    if (version.empty()) return false;

    out.banner = line;
    out.protocol = protocol;
    out.version = version;
    out.comments = comments;

    // "1.99" means "2.0 speaking, 1.x compatible"; both numbers are exposed so
    // `protocol_major < 2` catches an SSHv1-only server.
    out.protocol_major = 0;
    out.protocol_minor = 0;
    std::size_t pos = 0;
    if (detail::parse_uint(protocol, pos, out.protocol_major) && pos < protocol.size() && protocol[pos] == '.') {
      ++pos;
      detail::parse_uint(protocol, pos, out.protocol_minor);
    }

    detail::split_software(version, out.software, out.software_version);
    return true;
  }
  return false;
}

}  // namespace check_ssh_internal
}  // namespace check_net
