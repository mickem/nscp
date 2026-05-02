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

#include "op5_client.hpp"

#include <bytes/base64.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <net/http/client.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>

namespace json = boost::json;

op5_client::op5_client(const nscapi::core_wrapper *core, int plugin_id, op5_config config)
    : core_(core), plugin_id_(plugin_id), config_(config), stop_thread_(false) {
  thread_ = boost::shared_ptr<boost::thread>(new boost::thread([this]() { this->thread_proc(); }));
}

/**
 * Default d-tor
 * @return
 */
op5_client::~op5_client() {}

#define HTTP_HDR_AUTH "Authorization"
#define HTTP_HDR_AUTH_BASIC "Basic "

static inline bool is_2xx(const http::response &r) { return r.status_code_ >= 200 && r.status_code_ <= 299; }
static inline bool is_404(const http::response &r) { return r.status_code_ == 404; }
static inline std::string get_error(const http::response &r) {
  if (r.status_code_ == 0) return "Failed to connect to host";
  return str::xtos(r.status_code_) + ": " + r.payload_;
}

static std::string encode_b64(const std::string &input) {
  const size_t req = b64::b64_encode(input.c_str(), input.size(), nullptr, 0);
  std::string result(req, '\0');
  const size_t actual = b64::b64_encode(input.c_str(), input.size(), &result[0], req);
  result.resize(actual);
  return result;
}

std::string get_my_ip() {
  namespace bai = boost::asio::ip;
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);

  std::string h = boost::asio::ip::host_name();
  boost::optional<std::string> firstv4;
  boost::optional<std::string> firstv6;

  bai::tcp::resolver::iterator endpoint_iterator = resolver.resolve(boost::asio::ip::tcp::resolver::query(h, ""));
  bai::tcp::resolver::iterator end;

  for (; endpoint_iterator != end; endpoint_iterator++) {
    if (endpoint_iterator->endpoint().address().is_v6()) {
      if (!firstv6) {
        firstv6 = endpoint_iterator->endpoint().address().to_string();
      }
    } else {
      if (!firstv4) {
        firstv4 = endpoint_iterator->endpoint().address().to_string();
      }
    }
  }
  if (firstv4) {
    return *firstv4;
  } else if (firstv6) {
    return *firstv6;
  }
  return h;
}

http::response op5_client::do_call(const char *verb, const std::string &url, const std::string &payload) {
  std::string base_url;
  std::string auth_header;
  {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) {
      NSC_LOG_ERROR("Failed to read config");
      return http::response();
    }
    base_url = config_.url;
    const std::string uid = config_.username + ":" + config_.password;
    auth_header = std::string(HTTP_HDR_AUTH_BASIC) + encode_b64(uid);
  }
  const std::string full_url = base_url + url;
  NSC_TRACE_ENABLED() {
    NSC_TRACE_MSG(std::string(verb) + ": " + full_url);
    NSC_TRACE_MSG(std::string(HTTP_HDR_AUTH) + "=" + auth_header);
    if (!payload.empty()) {
      NSC_TRACE_MSG(payload);
    }
  }
  try {
    const http::parsed_url parsed = http::parse_url(full_url);
    http::http_client_options opts(parsed.protocol, "1.2+", "none", "");
    http::request rq(verb, parsed.host, parsed.path, payload);
    rq.add_header(HTTP_HDR_AUTH, auth_header);
    rq.add_header("Accept", "application/json");
    rq.add_header("Content-type", "application/json");
    if (!payload.empty()) {
      rq.add_header("Content-Length", str::xtos(payload.size()));
    }
    http::simple_client c(opts);
    const http::response ret = c.fetch(parsed.host, parsed.port, rq);
    NSC_TRACE_ENABLED() {
      NSC_TRACE_MSG(str::xtos(ret.status_code_) + ": " + ret.payload_);
      NSC_TRACE_MSG("------------------------");
    }
    return ret;
  } catch (const socket_helpers::socket_exception &e) {
    NSC_LOG_ERROR("HTTP call to " + full_url + " failed: " + e.reason());
    return http::response();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("HTTP call to " + full_url + " failed: " + utf8::utf8_from_native(e.what()));
    return http::response();
  }
}

bool op5_client::has_host(std::string host) {
  const http::response response = do_call("GET", "/api/filter/query?query=[hosts]%20name=\"" + host + "\"", "");
  if (!is_2xx(response)) {
    NSC_LOG_ERROR("Failed to check host: " + host + ": " + get_error(response));
    return false;
  }

  try {
    auto root = json::parse(response.payload_);
    return root.as_array().size() > 0;
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to parse response: " + utf8::utf8_from_native(e.what()));
    return false;
  }
  return false;
}

std::pair<bool, bool> op5_client::has_service(std::string service, std::string host, std::string &hosts_string) {
  const http::response response = do_call("GET", "/api/config/service/" + service, "");

  if (is_404(response)) {
    return std::pair<bool, bool>(false, false);
  }
  if (!is_2xx(response)) {
    NSC_LOG_ERROR("Failed to check host: " + service + ": " + get_error(response));
    return std::pair<bool, bool>(false, false);
  }

  try {
    auto root = json::parse(response.payload_).as_object();
    std::vector<std::string> hosts;
    hosts_string = root["host_name"].as_string().c_str();
    boost::split(hosts, hosts_string, boost::is_any_of(", "), boost::token_compress_on);
    if (std::find(hosts.begin(), hosts.end(), host) == hosts.end()) {
      return std::pair<bool, bool>(true, false);
    } else {
      return std::pair<bool, bool>(true, true);
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to parse response: " + response.payload_);
    return std::pair<bool, bool>(false, false);
  }
}

bool op5_client::add_host(std::string host, std::string hostgroups, std::string contactgroups) {
  json::object req;
  req["alias"] = host;
  req["host_name"] = host;
  req["address"] = get_my_ip();
  req["active_checks_enabled"] = 0;
  if (!hostgroups.empty()) {
    req["hostgroups"] = hostgroups;
  }
  if (!contactgroups.empty()) {
    req["contact_groups"] = contactgroups;
  }

  const http::response response = do_call("POST", "/api/config/host", json::serialize(req));

  if (!is_2xx(response)) {
    NSC_LOG_ERROR("Failed to add host: " + host + ": " + get_error(response));
    return false;
  }
  return true;
}

bool op5_client::remove_host(std::string host) {
  const http::response response = do_call("DELETE", "/api/config/host/" + host, "");

  if (!is_2xx(response)) {
    NSC_LOG_ERROR("Failed to delete host: " + host + ": " + get_error(response));
    return false;
  }
  return true;
}

bool op5_client::save_config() {
  const http::response response = do_call("POST", "/api/config/change", "");

  if (!is_2xx(response)) {
    NSC_LOG_ERROR("Failed to save configuration: " + get_error(response));
    return false;
  }
  return true;
}

bool op5_client::send_host_check(std::string host, int status_code, std::string msg, std::string &status, bool create_if_missing) {
  json::object req;
  req["host_name"] = host;
  req["status_code"] = status_code;
  req["plugin_output"] = msg;

  const http::response response = do_call("POST", "/api/command/PROCESS_HOST_CHECK_RESULT", json::serialize(req));

  if (!is_2xx(response)) {
    status = "Failed to submit host check to " + host + ": " + get_error(response);
    return false;
  }
  status = "Submitted host status to " + host;
  return true;
}

bool op5_client::send_service_check(std::string host, std::string service, int status_code, std::string msg, std::string &status, bool create_if_missing) {
  json::object req;
  req["host_name"] = host;
  req["service_description"] = service;
  req["status_code"] = status_code;
  req["plugin_output"] = msg;

  const http::response response = do_call("POST", "/api/command/PROCESS_SERVICE_CHECK_RESULT", json::serialize(req));

  if (is_404(response)) {
    if (create_if_missing) {
      add_service(host, service);
      save_config();
      return send_service_check(host, service, status_code, msg, status, false);
    }
    status = "Service " + service + " does not exist on " + host;
    return false;
  }
  if (!is_2xx(response)) {
    status = "Failed to submit " + service + " result: " + host + ": " + get_error(response);
    return false;
  }
  status = "Submitted " + service + " to " + host;
  return true;
}

bool op5_client::add_service(std::string host, std::string service) {
  json::object req;
  req["service_description"] = service;
  req["host_name"] = host;
  req["check_command"] = "bizproc_pas";
  req["active_checks_enabled"] = 0;
  req["freshness_threshold"] = 600;

  const http::response response = do_call("POST", "/api/config/service", json::serialize(req));

  if (!is_2xx(response)) {
    NSC_LOG_ERROR("Failed to add service " + service + " to " + host + ": " + get_error(response));
    return false;
  }
  return true;
}

bool op5_client::add_host_to_service(std::string service, std::string host, std::string &hosts_string) {
  json::object req;
  if (hosts_string.length() > 0) {
    hosts_string += "," + host;
  } else {
    hosts_string = host;
  }
  req["host_name"] = host;

  const http::response response = do_call("PATCH", "/api/config/service/" + service, json::serialize(req));

  if (!is_2xx(response)) {
    NSC_LOG_ERROR("Failed to add service " + service + " to " + host + ": " + get_error(response));
    return false;
  }
  return true;
}

void op5_client::register_host(std::string host, std::string hostgroups, std::string contactgroups) {
  if (!has_host(host)) {
    NSC_TRACE_MSG("Adding host");
    add_host(host, hostgroups, contactgroups);
    NSC_TRACE_MSG("Saving config");
    save_config();
  }
}

void op5_client::deregister_host(std::string host) {
  if (has_host(host)) {
    NSC_TRACE_MSG("Removing host");
    remove_host(host);
    NSC_TRACE_MSG("Saving config");
    save_config();
  }
}

void op5_client::add_check(std::string key, std::string arg) {
  try {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) {
      NSC_LOG_ERROR("Failed to add check: " + key);
      return;
    }
    config_.checks[key] = arg;
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add check: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add check: " + key);
  }
}

void op5_client::stop() {
  if (thread_) {
    stop_thread_ = true;
    thread_->interrupt();
    thread_->join();
  }
  thread_.reset();
}

void op5_client::thread_proc() {
  try {
    std::string hostname, hostgroups, contactgroups;
    bool deregister = false;
    unsigned long long interval = 3600;
    {
      boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
      if (!lock.owns_lock()) {
        NSC_LOG_ERROR("Failed to start thread");
        return;
      }
      hostname = config_.hostname;
      hostgroups = config_.hostgroups;
      contactgroups = config_.contactgroups;
      deregister = config_.deregister;
      interval = config_.interval;
    }
    register_host(hostname, hostgroups, contactgroups);

    while (true) {
      try {
        NSC_TRACE_MSG("Running op5 checks...");
        std::string status;
        if (!send_a_check("host_check", NSCAPI::query_return_codes::returnOK, "OK", status)) {
          NSC_LOG_ERROR("Failed to submit host ok status: " + status);
        }

        op5_config::check_map copy;
        {
          boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
          if (!lock.owns_lock()) {
            NSC_LOG_ERROR("Failed to run checks");
            continue;
          }
          interval = config_.interval;
          copy = config_.checks;
        }
        std::string response;
        nscapi::core_helper ch(get_core(), get_id());
        for (op5_config::check_map::value_type &v : copy) {
          std::string command;
          std::string alias = v.second;
          std::list<std::string> arguments;
          str::utils::parse_command(alias, command, arguments);

          if (ch.simple_query(command, arguments, response)) {
            PB::Commands::QueryResponseMessage resp_msg;
            resp_msg.ParseFromString(response);
            for (const PB::Commands::QueryResponseMessage::Response &p : resp_msg.payload()) {
              std::string message = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);
              int result = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
              std::string status;
              if (!send_a_check(v.first, result, message, status)) {
                NSC_LOG_ERROR("Failed to submit " + v.first + " result: " + status);
              }
            }
          } else {
            std::string status;
            if (!send_a_check(v.first, NSCAPI::query_return_codes::returnUNKNOWN, "Failed to execute command: " + command, status)) {
              NSC_LOG_ERROR("Failed to submit " + v.first + " result: " + status);
            }
          }
        }
      } catch (const std::exception &e) {
        NSC_LOG_ERROR_EXR("Failed to submit data: ", e);
      }
      if (stop_thread_) {
        if (deregister) {
          deregister_host(hostname);
        }
        return;
      }
      try {
        boost::this_thread::sleep(boost::posix_time::seconds(interval));
      } catch (const boost::thread_interrupted &e) {
        if (stop_thread_) {
          if (deregister) {
            deregister_host(hostname);
          }
          return;
        }
      }
    }
  } catch (...) {
    NSC_LOG_ERROR("Unknown exception in thread, op5 will not recieve requests");
  }
}

bool op5_client::send_a_check(const std::string &alias, int result, std::string message, std::string &status) {
  std::string hostname;
  {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) {
      status = "failed to fetch host name";
      return false;
    }
    hostname = config_.hostname;
  }

  if (alias == "host_check" || alias.empty()) {
    return send_host_check(hostname, result, message, status);
  } else {
    return send_service_check(hostname, alias, result, message, status);
  }
}
