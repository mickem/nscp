// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_webserver_internal.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <locale>
#include <map>
#include <sstream>

namespace check_net {
namespace check_webserver_internal {

namespace {

// Split "Key: value" / "key:   value" lines into a map. Both Apache's ?auto
// output and PHP-FPM's status page use this shape (FPM pads with spaces).
std::map<std::string, std::string> parse_colon_lines(const std::string &body) {
  std::map<std::string, std::string> out;
  std::istringstream stream(body);
  std::string line;
  while (std::getline(stream, line)) {
    const auto pos = line.find(':');
    if (pos == std::string::npos) continue;
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    boost::trim(key);
    boost::trim(value);
    if (!key.empty()) out[key] = value;
  }
  return out;
}

long long to_ll(const std::map<std::string, std::string> &values, const std::string &key) {
  const auto it = values.find(key);
  if (it == values.end()) return 0;
  try {
    return std::stoll(it->second);
  } catch (const std::exception &) {
    return 0;
  }
}

double to_dbl(const std::map<std::string, std::string> &values, const std::string &key) {
  const auto it = values.find(key);
  if (it == values.end()) return 0.0;
  // Not std::stod: that honours the process locale, and on a locale with a
  // decimal comma (e.g. Swedish) it would stop parsing "1.14985" at the dot.
  std::istringstream stream(it->second);
  stream.imbue(std::locale::classic());
  double value = 0.0;
  stream >> value;
  return stream.fail() ? 0.0 : value;
}

std::string to_str(const std::map<std::string, std::string> &values, const std::string &key) {
  const auto it = values.find(key);
  return it == values.end() ? "" : it->second;
}

long long attr_ll(const boost::property_tree::ptree &node, const std::string &name) { return node.get<long long>("<xmlattr>." + name, 0); }

}  // namespace

bool parse_apache_auto(const std::string &body, apache_status &out) {
  const auto values = parse_colon_lines(body);
  // BusyWorkers/IdleWorkers are present in every mod_status ?auto response;
  // their absence means we got something else (typically the HTML page).
  if (values.count("BusyWorkers") == 0 || values.count("IdleWorkers") == 0) return false;
  out.total_accesses = to_ll(values, "Total Accesses");
  out.total_kbytes = to_ll(values, "Total kBytes");
  out.uptime = to_ll(values, "Uptime");
  out.requests_per_sec = to_dbl(values, "ReqPerSec");
  out.bytes_per_sec = to_dbl(values, "BytesPerSec");
  out.busy_workers = to_ll(values, "BusyWorkers");
  out.idle_workers = to_ll(values, "IdleWorkers");
  out.scoreboard = to_str(values, "Scoreboard");
  return true;
}

bool parse_nginx_stub(const std::string &body, nginx_status &out) {
  // Format (fixed since nginx 0.1):
  //   Active connections: 291
  //   server accepts handled requests
  //    16630948 16630948 31070465
  //   Reading: 6 Writing: 179 Waiting: 106
  std::istringstream stream(body);
  std::string line;
  bool have_active = false, have_counters = false, have_states = false;
  while (std::getline(stream, line)) {
    if (line.rfind("Active connections:", 0) == 0) {
      try {
        out.active = std::stoll(line.substr(19));
        have_active = true;
      } catch (const std::exception &) {
        return false;
      }
    } else if (line.rfind("Reading:", 0) == 0) {
      std::istringstream states(line);
      std::string label;
      // "Reading: 6 Writing: 179 Waiting: 106"
      if (!(states >> label >> out.reading >> label >> out.writing >> label >> out.waiting)) return false;
      have_states = true;
    } else if (line.find("accepts handled requests") != std::string::npos) {
      if (!std::getline(stream, line)) return false;
      std::istringstream counters(line);
      if (!(counters >> out.accepts >> out.handled >> out.requests)) return false;
      have_counters = true;
    }
  }
  return have_active && have_counters && have_states;
}

bool parse_phpfpm_status(const std::string &body, phpfpm_status &out) {
  const auto values = parse_colon_lines(body);
  if (values.count("pool") == 0 || values.count("accepted conn") == 0) return false;
  out.pool = to_str(values, "pool");
  out.process_manager = to_str(values, "process manager");
  out.accepted_conn = to_ll(values, "accepted conn");
  out.listen_queue = to_ll(values, "listen queue");
  out.max_listen_queue = to_ll(values, "max listen queue");
  out.listen_queue_len = to_ll(values, "listen queue len");
  out.idle_processes = to_ll(values, "idle processes");
  out.active_processes = to_ll(values, "active processes");
  out.total_processes = to_ll(values, "total processes");
  out.max_active_processes = to_ll(values, "max active processes");
  out.max_children_reached = to_ll(values, "max children reached");
  out.slow_requests = to_ll(values, "slow requests");
  return true;
}

bool parse_tomcat_status_xml(const std::string &body, tomcat_status &out) {
  namespace pt = boost::property_tree;
  pt::ptree tree;
  try {
    std::istringstream stream(body);
    pt::read_xml(stream, tree);
  } catch (const pt::xml_parser_error &) {
    return false;
  }
  const auto status = tree.get_child_optional("status");
  if (!status) return false;
  if (const auto memory = status->get_child_optional("jvm.memory")) {
    out.memory_free = attr_ll(*memory, "free");
    out.memory_total = attr_ll(*memory, "total");
    out.memory_max = attr_ll(*memory, "max");
  }
  for (const auto &child : *status) {
    if (child.first != "connector") continue;
    tomcat_connector c;
    // Tomcat quotes the name attribute value itself: name='"http-nio-8080"'.
    c.name = child.second.get<std::string>("<xmlattr>.name", "");
    boost::trim_if(c.name, boost::is_any_of("\""));
    if (const auto threads = child.second.get_child_optional("threadInfo")) {
      c.threads_max = attr_ll(*threads, "maxThreads");
      c.threads_current = attr_ll(*threads, "currentThreadCount");
      c.threads_busy = attr_ll(*threads, "currentThreadsBusy");
    }
    if (const auto requests = child.second.get_child_optional("requestInfo")) {
      c.max_time = attr_ll(*requests, "maxTime");
      c.processing_time = attr_ll(*requests, "processingTime");
      c.request_count = attr_ll(*requests, "requestCount");
      c.error_count = attr_ll(*requests, "errorCount");
      c.bytes_received = attr_ll(*requests, "bytesReceived");
      c.bytes_sent = attr_ll(*requests, "bytesSent");
    }
    out.connectors.push_back(c);
  }
  return !out.connectors.empty() || status->get_child_optional("jvm").is_initialized();
}

}  // namespace check_webserver_internal
}  // namespace check_net
