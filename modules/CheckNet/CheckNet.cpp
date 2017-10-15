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

#include "CheckNet.h"

#include <parsers/expression/expression.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/helpers.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <net/pinger.hpp>

#include "filter.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

void CheckNet::check_ping(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;
	modern_filter::cli_helper<ping_filter::filter> filter_helper(request, response, data);
	std::vector<std::string> hosts;
	std::string hosts_string;
	bool total = false;
	int count = 0;
	int timeout = 0;

	ping_filter::filter filter;
	filter_helper.add_options("time > 60 or loss > 5%", "time > 100 or loss > 10%", "", filter.get_filter_syntax(), "unknown");
	filter_helper.add_syntax("${status}: ${ok_count}/${count} (${problem_list})", "${ip} Packet loss = ${loss}%, RTA = ${time}ms", "${host}", "No hosts found", "%(status): All %(count) hosts are ok");
	filter_helper.get_desc().add_options()
		("host", po::value<std::vector<std::string> >(&hosts),
			"The host to check (or multiple hosts).")
		("total", po::bool_switch(&total), "Include the total of all matching hosts")
		("hosts", po::value<std::string>(&hosts_string),
			"The host to check (or multiple hosts).")
		("count", po::value<int>(&count)->default_value(1),
			"Number of packets to send.")
		("timeout", po::value<int>(&timeout)->default_value(500),
			"Timeout in milliseconds.")
		;

	if (!filter_helper.parse_options())
		return;

	if (!hosts_string.empty())
		boost::split(hosts, hosts_string, boost::is_any_of(","));

	if (hosts.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No host specified");
	if (hosts.size() == 1)
		filter_helper.show_all = true;

	if (!filter_helper.build_filter(filter))
		return;

	boost::shared_ptr<ping_filter::filter_obj> total_obj;
	if (total)
		total_obj = ping_filter::filter_obj::get_total();

	BOOST_FOREACH(const std::string &host, hosts) {
		result_container result;
		for (int i = 0; i < count; i++) {
			boost::asio::io_service io_service;
			pinger ping(io_service, result, host.c_str(), timeout);
			ping.ping();
			io_service.run();
		}
		boost::shared_ptr<ping_filter::filter_obj> obj = boost::make_shared<ping_filter::filter_obj>(result);
		filter.match(obj);
		if (total_obj)
			total_obj->add(obj);
	}
	if (total_obj)
		filter.match(total_obj);
	filter_helper.post_process(filter);
}