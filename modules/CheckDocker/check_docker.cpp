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

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/filter/cli_helper.hpp>

#include <str/format.hpp>
#include <http/client.hpp>

#include <json_spirit.h>

#include <string>

namespace check_docker_filter {
	namespace ph = boost::placeholders;
	struct filter_obj {
		std::string id, image ,imageId, command, created, state, status, names, ip;

		filter_obj(json_spirit::Value& v) {

			id = v.getString("Id");
			image = v.getString("Image");
			imageId = v.getString("ImageID");
			command = v.getString("Command");
			//created = v.getString("Created");
			state = v.getString("State");
			status = v.getString("Status");
			for(const json_spirit::Value & name: v.getArray("Names")) {
				str::format::append_list(names, name.getString(), ",");
			}
			json_spirit::Value netSettings = v.getObject("NetworkSettings");
			if (netSettings.contains("Networks")) {
				json_spirit::Value net = netSettings.getObject("Networks");
				if (net.contains("bridge")) {
					json_spirit::Value bridge = net.getObject("bridge");
					if (bridge.contains("IPAddress")) {
						ip = bridge.getString("IPAddress");
					}
				}
			}

			// List of objects: Ports
			// Object: Labels
			// Object: HostConfig
			// List of objects: Mounts

		}

		std::string get_id() const {
			return id;
		}
		std::string get_image() const {
			return image;
		}
		std::string get_imageId() const {
			return imageId;
		}
		std::string get_command() const {
			return command;
		}
		std::string get_state() const {
			return state;
		}
		std::string get_status() const {
			return status;
		}
		std::string get_names() const {
			return names;
		}
		std::string get_ip() const {
			return ip;
		}


	};




	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {

		filter_obj_handler() {
			static const parsers::where::value_type type_custom_used = parsers::where::type_custom_int_1;
			static const parsers::where::value_type type_custom_free = parsers::where::type_custom_int_2;

                        // clang-format off

			registry_.add_string()
				("id", boost::bind(&filter_obj::get_id, ph::_1), "Container id")
				("image", boost::bind(&filter_obj::get_image, ph::_1), "Container image")
				("image_id", boost::bind(&filter_obj::get_imageId, ph::_1), "Container image id")
				("command", boost::bind(&filter_obj::get_command, ph::_1), "Command")
				("container_state", boost::bind(&filter_obj::get_state, ph::_1), "Container image")
				("status", boost::bind(&filter_obj::get_status, ph::_1), "Container image")
				("names", boost::bind(&filter_obj::get_names, ph::_1), "Container image")
				("ip", boost::bind(&filter_obj::get_ip, ph::_1), "IP of container")
				;

// clang-format on
				
		}
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}

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
	void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
		typedef check_docker_filter::filter filter_type;
		modern_filter::data_container data;
		modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
		std::string host = "\\\\.\\pipe\\docker_engine";

		filter_type filter;
		filter_helper.add_options("container_state != 'running'", "container_state != 'running'", "", filter.get_filter_syntax(), "warning");
		filter_helper.add_syntax("${status}: ${list}", "${names}=${container_state}", "${id}", "", "");
		filter_helper.get_desc().add_options()
			("host", po::value<std::string>(&host), "The host or socket of the docker deamon")
			;

		if (!filter_helper.parse_options())
			return;

		if (!filter_helper.build_filter(filter))
			return;

		try {
			http::packet rq("GET", "", "/v1.40/containers/json");

			std::stringstream ss;
			std::string error;


			rq.add_default_headers();
			http::simple_client c("pipe");
			c.execute(ss, host, "", rq);


			json_spirit::Value root;
			json_spirit::read_or_throw(ss.str(), root);
			json_spirit::Array list = root.getArray();
			for(json_spirit::Value & v: list) {
				boost::shared_ptr<check_docker_filter::filter_obj> record(new check_docker_filter::filter_obj(v));
				filter.match(record);

			}

		}
		catch (const socket_helpers::socket_exception& e) {
			NSC_LOG_ERROR(e.reason());
		}
		catch (const std::exception& e) {
			std::string error_msg = std::string("Exception: ") + utf8::utf8_from_native(e.what());
			NSC_LOG_ERROR(error_msg);
		}


		filter_helper.post_process(filter);
	}
}
