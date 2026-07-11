// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace share_check {

// One SMB share (Win32_Share). `exists` is false for a share requested via
// `share=` that is not actually present, so a filter can alert on it.
struct share_info {
  std::string name;         // share name (e.g. "C$", "Public")
  std::string path;         // local path the share maps to (empty for IPC$)
  std::string description;  // share comment/description
  long long type_raw;       // Win32_Share.Type (low byte = kind, high bit = admin)
  bool exists;              // false only for a requested-but-missing share

  share_info() : type_raw(0), exists(false) {}

  // Map the low byte of Win32_Share.Type to a short kind string.
  static std::string type_to_string(long long t) {
    switch (t & 0xFF) {
      case 0:
        return "disk";
      case 1:
        return "printer";
      case 2:
        return "device";
      case 3:
        return "ipc";
      default:
        return "unknown";
    }
  }

  std::string get_name() const { return name; }
  std::string get_path() const { return path; }
  std::string get_description() const { return description; }
  std::string get_type() const { return type_to_string(type_raw); }
  // The high bit (0x80000000) flags an administrative share (C$, ADMIN$, IPC$).
  long long get_is_admin() const { return (static_cast<unsigned long long>(type_raw) & 0x80000000ULL) != 0 ? 1 : 0; }
  long long get_exists() const { return exists ? 1 : 0; }

  std::string show() const { return name; }
};

typedef std::list<share_info> shares_type;

// Platform data acquisition: WMI (Win32_Share) on Windows, empty on Unix.
shares_type query();

namespace check {
void check_share(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check

}  // namespace share_check
