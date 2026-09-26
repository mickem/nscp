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

#include "plist_value.h"

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
  std::string manager;           // dpkg, rpm, pacman; on macOS pkgutil, bundle or homebrew
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

// Result of running one package-manager query. `ok` is false when the command
// could not be started or exited non-zero: an empty package list is then a
// query failure, not an empty package database, and must not be reported as a
// clean "no software installed" inventory.
struct command_result {
  std::string output;
  bool ok;

  command_result() : ok(false) {}
  command_result(std::string out, const bool success) : output(std::move(out)), ok(success) {}
};

// Helper for command execution (injectable for tests).
typedef std::function<command_result(const std::string &)> exec_fn;

// The real one: runs the command through popen, captures stdout and keeps the
// exit status. Exposed because the `software.installed` fact set runs the same
// queries this check does (software_facts_unix.cpp) and there is no reason for
// a second popen wrapper to exist.
command_result run_command(const std::string &cmd);

// The package manager owning this host: the manager name plus the absolute
// path of its query binary (commands are invoked by absolute path so a
// manipulated PATH cannot redirect them).
struct package_manager {
  std::string name;    // dpkg, rpm, pacman, or macos (several sources, see fetch_macos_inventory)
  std::string binary;  // absolute path to the query binary

  bool empty() const { return name.empty(); }
};

// The installed-package list plus whether the underlying query succeeded.
struct fetch_result {
  std::vector<software_entry> entries;
  bool ok;

  fetch_result() : ok(false) {}
};

// Render an epoch as YYYY-MM-DD (UTC); empty for epoch <= 0.
std::string format_epoch_date(long long epoch);

// Pure parsers (exposed for unit tests). Each returns the installed-package
// list with manager set; entries the manager reports as not fully installed
// (dpkg config-files leftovers etc.) are skipped.
std::vector<software_entry> parse_dpkg_output(const std::string &output);
std::vector<software_entry> parse_rpm_output(const std::string &output);
std::vector<software_entry> parse_pacman_output(const std::string &output);

// Detection of the available package manager (name is empty if none found).
// Order: macOS (pkgutil), dpkg-query, rpm, pacman — macOS first because a Mac
// can carry a Homebrew dpkg or rpm that does not own the system, and dpkg
// before rpm because Debian-family hosts frequently have an rpm binary
// installed as well.
package_manager detect_manager();

// ---- macOS -------------------------------------------------------------------
// A Mac has no single package database. Its inventory is three sources, each
// entry tagged with the one it came from in `manager`:
//   pkgutil   installer receipts in /var/db/receipts (what `pkgutil --pkgs`
//             lists): identifier, version and install date.
//   bundle    application bundles in /Applications and /Applications/Utilities:
//             the bundle name, its version and when it was last modified.
//   homebrew  Homebrew formulae and casks, read from the Cellar and Caskroom
//             directories rather than by running brew.
// Everything is read in-process; nothing forks.

// An installer receipt's property list as a row. Empty name when it is not a
// receipt.
software_entry receipt_entry(const plist::value &receipt);

// An application bundle as a row: `bundle_name` is the .app directory without
// the extension, `info` its Contents/Info.plist, `mtime` when it last changed.
software_entry bundle_entry(const std::string &bundle_name, const plist::value &info, long long mtime);

// The three macOS sources together. Defined per platform; not ok on anything
// but macOS.
fetch_result fetch_macos_inventory();

// Run the query + parse pipeline for the given manager. exec is called with
// the shell command and must return the captured stdout plus whether the
// command succeeded.
fetch_result fetch_installed(const package_manager &manager, const exec_fn &exec);

// Testable core: renders / thresholds a pre-gathered entry list.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::vector<software_entry> &entries);

// Public check entry point.
void check_installed_software(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace installed_software

#endif  // NSCP_CHECK_INSTALLED_SOFTWARE_H
