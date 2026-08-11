// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_INSTALLED_SOFTWARE_H
#define NSCP_CHECK_INSTALLED_SOFTWARE_H

#include <functional>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace installed_software {

// One installed package as reported by the system package manager. The unix
// counterpart to the Windows registry-based check_installed_software; the
// shared keywords (name, version, publisher, install_date, size, architecture)
// carry the same meaning on both platforms.
struct software_entry {
  std::string name;              // Package name
  std::string version;           // Version (rpm: version-release)
  std::string publisher;         // Maintainer (dpkg) / vendor (rpm); may be empty
  std::string architecture;      // amd64, x86_64, noarch, ...; may be empty
  std::string manager;           // dpkg, rpm, pacman
  std::string status;            // Package state; "installed" for everything this check lists
  std::string install_date_str;  // YYYY-MM-DD when known; empty otherwise
  long long install_date_epoch;  // Install time (epoch seconds); 0 if unknown
  long long size_bytes;          // Installed size in bytes; 0 if unknown

  software_entry() : status("installed"), install_date_epoch(0), size_bytes(0) {}

  std::string get_name() const { return name; }
  std::string get_version() const { return version; }
  std::string get_publisher() const { return publisher; }
  std::string get_architecture() const { return architecture; }
  std::string get_manager() const { return manager; }
  std::string get_status() const { return status; }
  std::string get_install_date_str() const { return install_date_str; }
  long long get_install_date() const { return install_date_epoch; }
  long long get_size() const { return size_bytes; }

  std::string show() const { return name + " " + version; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<software_entry> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<software_entry, filter_obj_handler> filter;

// Helpers for command execution / filesystem access (injectable for tests).
typedef std::function<std::string(const std::string &)> exec_fn;
typedef std::function<long long(const std::string &)> mtime_fn;

// Render an epoch as YYYY-MM-DD (UTC); empty for epoch <= 0.
std::string format_epoch_date(long long epoch);

// Pure parsers (exposed for unit tests). Each returns the installed-package
// list with manager set; entries the manager reports as not fully installed
// (dpkg config-files leftovers etc.) are skipped.
std::vector<software_entry> parse_dpkg_output(const std::string &output);
std::vector<software_entry> parse_rpm_output(const std::string &output);
std::vector<software_entry> parse_pacman_output(const std::string &output);

// dpkg does not record install dates; approximate them from the mtime of the
// package's /var/lib/dpkg/info/<name>[:<arch>].list file. mtime must return
// epoch seconds, or 0 when the path does not exist.
void apply_dpkg_install_dates(std::vector<software_entry> &entries, const mtime_fn &mtime);

// Detection of the available package manager (returns empty string if none
// found). Order: dpkg-query, rpm, pacman — dpkg first because Debian-family
// hosts frequently have an rpm binary installed as well.
std::string detect_manager();

// Run the query + parse pipeline for the given manager. exec is called with
// the shell command and must return the captured stdout.
std::vector<software_entry> fetch_installed(const std::string &manager, const exec_fn &exec);

// Testable core: renders / thresholds a pre-gathered entry list.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::vector<software_entry> &entries);

// Public check entry point.
void check_installed_software(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace installed_software

#endif  // NSCP_CHECK_INSTALLED_SOFTWARE_H
