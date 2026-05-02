/*
 * Copyright (C) 2004-2016 Michael Medin
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
#include <vector>

namespace icinga {

struct submit_result {
  bool ok;
  std::string message;
};

/// Translate an NSCP/Nagios status (0..3) into the Icinga 2 exit_status that
/// the process-check-result endpoint will accept.  See apiactions.cpp on the
/// Icinga 2 master branch: services accept 0..3, hosts accept only 0 or 1.
int map_exit_status(int nagios_status, bool is_host);

/// Split a Nagios performance-data string (already rendered as space separated
/// `label=value;...` tokens, possibly containing single-quoted labels with
/// embedded spaces) into individual entries to submit as a JSON array.
std::vector<std::string> split_perfdata(const std::string &perfdata);

/// Build the JSON body for a process-check-result action using the filter
/// form (`type` + `filter`).  An empty `service` means the result is a host
/// check, which also drives the exit_status clamping mandated by Icinga 2's
/// apiactions.cpp (host accepts only 0 or 1).
std::string build_check_result_body(int nagios_status, const std::string &plugin_output, const std::string &perfdata, const std::string &check_source,
                                    const std::string &host, const std::string &service);

/// Build a JSON body for `PUT /v1/objects/hosts/<host>` when ensuring objects.
std::string build_host_create_body(const std::string &host, const std::string &templates);

/// Build a JSON body for `PUT /v1/objects/services/<host>!<service>`.
std::string build_service_create_body(const std::string &templates, const std::string &check_command);

/// Parse the Icinga 2 process-check-result envelope.  Returns
///   ok=true  + status text  on success (HTTP 200 from `results[0].code`).
///   ok=false + status text  on failure (any other code).
submit_result parse_check_result_response(const std::string &body);

/// URL encode an Icinga 2 object identifier (host, service or `host!service`)
/// for inclusion in a path or a query string.
std::string url_encode(const std::string &value);

}  // namespace icinga
