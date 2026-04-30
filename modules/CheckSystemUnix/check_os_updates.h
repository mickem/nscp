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

#ifndef NSCP_CHECK_OS_UPDATES_H
#define NSCP_CHECK_OS_UPDATES_H

#include <boost/shared_ptr.hpp>
#include <functional>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

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
  std::string manager;                   // Package manager: apt, dnf, yum, zypper, pacman, none, unknown
  long long count;                       // Total number of available updates
  long long security;                    // Number of security updates
  std::vector<package_update> packages;  // List of available updates

  filter_obj() : count(0), security(0) {}

  std::string get_manager() const { return manager; }
  long long get_count() const { return count; }
  long long get_security() const { return security; }
  std::string get_packages() const;  // Comma separated package names

  std::string show() const;
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

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

// Detection of the available package manager (returns empty string if none found).
// Looks for binaries on PATH using access(2). Order: apt-get, dnf, yum, zypper, pacman.
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
