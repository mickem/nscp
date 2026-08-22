// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

namespace check_net {
namespace check_webserver_internal {

// Parsed Apache mod_status "?auto" output. Only the machine-readable keys are
// kept; unknown keys are ignored so newer Apache versions parse cleanly.
struct apache_status {
  long long total_accesses = 0;
  long long total_kbytes = 0;
  long long uptime = 0;
  double requests_per_sec = 0.0;
  double bytes_per_sec = 0.0;
  long long busy_workers = 0;
  long long idle_workers = 0;
  std::string scoreboard;
};

// Parsed nginx stub_status output.
struct nginx_status {
  long long active = 0;
  long long reading = 0;
  long long writing = 0;
  long long waiting = 0;
  long long accepts = 0;
  long long handled = 0;
  long long requests = 0;
};

// Parsed PHP-FPM status page (default text format).
struct phpfpm_status {
  std::string pool;
  std::string process_manager;
  long long accepted_conn = 0;
  long long listen_queue = 0;
  long long max_listen_queue = 0;
  long long listen_queue_len = 0;
  long long idle_processes = 0;
  long long active_processes = 0;
  long long total_processes = 0;
  long long max_active_processes = 0;
  long long max_children_reached = 0;
  long long slow_requests = 0;
};

// One connector from the Tomcat manager XML status page.
struct tomcat_connector {
  std::string name;
  long long threads_max = 0;
  long long threads_current = 0;
  long long threads_busy = 0;
  long long max_time = 0;
  long long processing_time = 0;
  long long request_count = 0;
  long long error_count = 0;
  long long bytes_received = 0;
  long long bytes_sent = 0;
};

// Parsed Tomcat "/manager/status?XML=true" page: JVM heap plus one entry per
// connector.
struct tomcat_status {
  long long memory_free = 0;
  long long memory_total = 0;
  long long memory_max = 0;
  std::vector<tomcat_connector> connectors;
};

// All parsers return false when the body does not look like the expected
// status format (e.g. an HTML error page); callers turn that into a
// "parse_error" check result rather than reporting bogus zeros as OK.
bool parse_apache_auto(const std::string &body, apache_status &out);
bool parse_nginx_stub(const std::string &body, nginx_status &out);
bool parse_phpfpm_status(const std::string &body, phpfpm_status &out);
bool parse_tomcat_status_xml(const std::string &body, tomcat_status &out);

// Ensure the vendor query parameter that switches the endpoint into its
// machine-readable format is present, appending it when missing (e.g. "auto"
// for Apache, "XML=true" for Tomcat). A URL that already carries the parameter
// is returned unchanged.
inline std::string ensure_query_param(const std::string &url, const std::string &param) {
  const auto qpos = url.find('?');
  if (qpos == std::string::npos) return url + "?" + param;
  // Compare only the parameter name so "XML=true" also matches "XML=false"
  // (the user asked for something explicit; leave it alone).
  const std::string name = param.substr(0, param.find('='));
  std::string query = url.substr(qpos + 1);
  std::string::size_type start = 0;
  while (start <= query.size()) {
    auto end = query.find('&', start);
    if (end == std::string::npos) end = query.size();
    std::string part = query.substr(start, end - start);
    const std::string part_name = part.substr(0, part.find('='));
    if (part_name == name) return url;
    start = end + 1;
  }
  return url + "&" + param;
}

}  // namespace check_webserver_internal
}  // namespace check_net
