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

#include "check_docker.hpp"

#include <boost/json.hpp>
#include <http/client.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <string>

namespace json = boost::json;

namespace check_docker_filter {
struct filter_obj {
  std::string id, image, imageId, command, created, state, status, names, ip;

  filter_obj(json::value& v) {
    auto o = v.as_object();
    id = o["Id"].as_string().c_str();
    image = o["Image"].as_string().c_str();
    imageId = o["ImageID"].as_string().c_str();
    command = o["Command"].as_string().c_str();
    // created = v.getString("Created");
    state = o["State"].as_string().c_str();
    status = o["Status"].as_string().c_str();
    for (const auto& name : o["Names"].as_array()) {
      str::format::append_list(names, name.as_string().c_str(), ",");
    }
    auto netSettings = o["NetworkSettings"].as_object();
    if (netSettings.contains("Networks")) {
      auto net = netSettings["Networks"].as_object();
      if (net.contains("bridge")) {
        auto bridge = net["bridge"].as_object();
        if (bridge.contains("IPAddress")) {
          ip = bridge["IPAddress"].as_string().c_str();
        }
      }
    }

    // List of objects: Ports
    // Object: Labels
    // Object: HostConfig
    // List of objects: Mounts
  }

  std::string get_id() const { return id; }
  std::string get_image() const { return image; }
  std::string get_imageId() const { return imageId; }
  std::string get_command() const { return command; }
  std::string get_state() const { return state; }
  std::string get_status() const { return status; }
  std::string get_names() const { return names; }
  std::string get_ip() const { return ip; }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    static const parsers::where::value_type type_custom_used = parsers::where::type_custom_int_1;
    static const parsers::where::value_type type_custom_free = parsers::where::type_custom_int_2;

    // clang-format off
    registry_.add_string()
        ("id", [](auto obj, auto context) { return obj->get_id();  }, "Container id")
      ("image", [](auto obj, auto context) { return obj->get_image();  }, "Container image")
      ("image_id", [](auto obj, auto context) { return obj->get_imageId();  }, "Container image id")
      ("command", [](auto obj, auto context) { return obj->get_command();  }, "Command")
      ("container_state", [](auto obj, auto context) { return obj->get_state();  }, "Container image")
      ("status", [](auto obj, auto context) { return obj->get_status();  }, "Container image")
      ("names", [](auto obj, auto context) { return obj->get_names();  }, "Container image")
      ("ip", [](auto obj, auto context) { return obj->get_ip();  }, "IP of container")
    ;
    // clang-format on
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_docker_filter

namespace docker_checks {

namespace po = boost::program_options;
/**
 * Check available memory and return various check results
 * Example: checkMem showAll maxWarn=50 maxCrit=75
 *
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in
 * @return The status of the command
 */
void check(const PB::Commands::QueryRequestMessage::Request& request, PB::Commands::QueryResponseMessage::Response* response) {
  typedef check_docker_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::string host = "\\\\.\\pipe\\docker_engine";

  filter_type filter;
  filter_helper.add_options("container_state != 'running'", "container_state != 'running'", "", filter.get_filter_syntax(), "warning");
  filter_helper.add_syntax("${status}: ${list}", "${names}=${container_state}", "${id}", "", "");
  filter_helper.get_desc().add_options()("host", po::value<std::string>(&host), "The host or socket of the docker deamon");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  try {
    http::packet rq("GET", "", "/v1.40/containers/json");

    std::stringstream ss;
    std::string error;

    rq.add_default_headers();
    http::http_client_options options("pipe", "", "", "");
    http::simple_client c(options);
    c.execute(ss, host, "", rq);

    auto root = json::parse(ss.str());
    json::array list = root.as_array();
    for (auto& v : list) {
      boost::shared_ptr<check_docker_filter::filter_obj> record(new check_docker_filter::filter_obj(v));
      filter.match(record);
    }

  } catch (const socket_helpers::socket_exception& e) {
    NSC_LOG_ERROR(e.reason());
  } catch (const std::exception& e) {
    std::string error_msg = std::string("Exception: ") + utf8::utf8_from_native(e.what());
    NSC_LOG_ERROR(error_msg);
  }

  filter_helper.post_process(filter);
}
}  // namespace docker_checks
