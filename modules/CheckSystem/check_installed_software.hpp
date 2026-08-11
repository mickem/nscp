// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace installed_software_check {

// One installed program as recorded under a CurrentVersion\Uninstall key.
// Deliberately sourced from the registry, never Win32_Product: querying that
// WMI class triggers an MSI consistency check that can reconfigure every
// installed package on the host.
struct software_entry {
  std::string name;              // DisplayName
  std::string version;           // DisplayVersion (may be empty)
  std::string publisher;         // Publisher (may be empty)
  std::string install_date_str;  // raw InstallDate string (usually YYYYMMDD; often empty)
  long long install_date_epoch;  // parsed install date (epoch seconds, 00:00 UTC); 0 if unknown
  std::string install_location;  // InstallLocation (may be empty)
  std::string uninstall_string;  // UninstallString (may be empty)
  long long size_kb;             // EstimatedSize (KB); 0 if not recorded
  std::string hive;              // "machine" (HKLM) or "user" (per-user hive under HKU)
  std::string user;              // account (DOMAIN\name) or SID of a per-user install; empty for machine-wide
  std::string architecture;      // "x64" / "x86" (HKLM registry view); empty for per-user installs
  std::string key;               // Uninstall sub-key name (product GUID or slug)
  bool system_component;         // SystemComponent=1 (hidden from Add/Remove Programs)
  bool windows_installer;        // WindowsInstaller=1 (installed via MSI)

  software_entry() : install_date_epoch(0), size_kb(0), system_component(false), windows_installer(false) {}

  std::string get_name() const { return name; }
  std::string get_version() const { return version; }
  std::string get_publisher() const { return publisher; }
  std::string get_install_date_str() const { return install_date_str; }
  long long get_install_date() const { return install_date_epoch; }
  std::string get_install_location() const { return install_location; }
  std::string get_uninstall_string() const { return uninstall_string; }
  long long get_size() const { return size_kb * 1024LL; }  // bytes, so type_size units work
  std::string get_hive() const { return hive; }
  std::string get_user() const { return user; }
  std::string get_architecture() const { return architecture; }
  std::string get_key() const { return key; }
  long long get_system_component() const { return system_component ? 1 : 0; }
  long long get_windows_installer() const { return windows_installer ? 1 : 0; }

  std::string show() const { return name + " " + version; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<software_entry> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<software_entry, filter_obj_handler> filter_type;

// Testable core: renders / thresholds a pre-gathered entry list. Pure with
// respect to the registry so it can be unit tested with synthetic entries.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::vector<software_entry> &entries);

// Walk the Uninstall hives: HKLM in both the 64-bit and 32-bit (Wow6432Node)
// views, plus every loaded per-user hive under HKEY_USERS (which covers HKCU
// regardless of the account the service runs as). Entries without a
// DisplayName and legacy patch children (ParentKeyName set) are skipped.
std::vector<software_entry> gather_installed_software();

// Live check: enumerates the registry and thresholds the result.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace installed_software_check
