// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_OS_UPDATES_H
#define NSCP_CHECK_OS_UPDATES_H

#include <functional>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <boost/optional.hpp>
#include <string>
#include <vector>

#include "plist_value.h"

namespace os_updates {

// Information about a single available package update.
struct package_update {
  std::string name;     // Package name
  std::string version;  // New version (may be empty)
  std::string source;   // Source/repo (may be empty)
  bool security;        // True if this update is from a security source

  package_update() : security(false) {}
  package_update(const std::string &n, const std::string &v, const std::string &s, bool sec) : name(n), version(v), source(s), security(sec) {}
};

// Aggregated update information used as the filter object.
struct filter_obj {
  std::string manager;                   // Package manager: apt, dnf, yum, zypper, pacman, softwareupdate, none, unknown
  long long count;                       // Total number of available updates
  long long security;                    // Number of security updates
  std::vector<package_update> packages;  // List of available updates
  // When the list was last refreshed from the update server, where the
  // manager records it (macOS's cached list does); 0 when unknown.
  long long last_checked;

  filter_obj() : count(0), security(0), last_checked(0) {}

  std::string get_manager() const { return manager; }
  long long get_count() const { return count; }
  long long get_security() const { return security; }
  boost::optional<long long> get_last_checked() const { return last_checked > 0 ? boost::optional<long long>(last_checked) : boost::none; }
  std::string get_packages() const;  // Comma separated package names

  std::string show() const;
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Pure parsers (exposed for unit tests).
// All parsers return a filter_obj with manager and packages populated; count and security are
// derived from the packages list.
filter_obj parse_apt_output(const std::string &output);
filter_obj parse_dnf_output(const std::string &output);
filter_obj parse_zypper_output(const std::string &output);
filter_obj parse_pacman_output(const std::string &output);

// macOS. The cached list is /Library/Preferences/com.apple.SoftwareUpdate.plist,
// which macOS rewrites at every background check: RecommendedUpdates holds
// the updates and LastSuccessfulDate when it last reached the server. Reading
// it is instant and needs no network. `softwareupdate --list` asks the server
// live (10 to 60 seconds).
//
// There is no security classification on either; an update is counted as a
// security update when its name or identifier says so (the Rapid and
// Background Security Responses, the old Security Update packages). A macOS
// point release carries security fixes too and is not counted.
filter_obj parse_software_update_plist(const plist::value &plist);
filter_obj parse_softwareupdate_output(const std::string &output);

// The cached list's property list. Defined per platform: empty anywhere but
// macOS.
plist::value read_software_update_cache();

// Detection of the available package manager (returns empty string if none found).
// Looks for binaries on PATH using access(2). Order: softwareupdate (macOS),
// apt-get, dnf, yum, zypper, pacman.
std::string detect_manager();

// Helpers for command execution.
typedef std::function<std::string(const std::string &)> exec_fn;

// Run the appropriate detection + parse pipeline. exec is called with the shell command and
// must return the captured stdout.
filter_obj fetch_updates(const std::string &manager, const exec_fn &exec);

// Public check entry point.
void check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace os_updates

#endif  // NSCP_CHECK_OS_UPDATES_H
