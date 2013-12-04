/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>

#include "CheckHelpers.h"
#include <strEx.h>
#include <time.h>
#include <vector>
#include <algorithm>

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

void check_simple_status(::Plugin::Common_ResultCode status, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response)  {
	po::options_description desc = nscapi::program_options::create_desc(request);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	response->set_result(status);
	if (request.arguments_size() == 0) {
		response->set_message("No message.");
	} else {
		std::string msg;
		for (int i=0;i<request.arguments_size();i++)
			msg += request.arguments(i);
		response->set_message(msg);
	}
}

void escalate_result(Plugin::QueryResponseMessage::Response * response, ::Plugin::Common_ResultCode result)
{
	if (response->result() == result)
		return;
	else if (response->result() == Plugin::Common_ResultCode_OK && result != Plugin::Common_ResultCode_OK)
		response->set_result(result);
	else if (response->result() == Plugin::Common_ResultCode_OK)
		return;
	else if (response->result() == Plugin::Common_ResultCode_WARNING && result != Plugin::Common_ResultCode_WARNING)
		response->set_result(result);
	else if (response->result() == Plugin::Common_ResultCode_WARNING)
		return;
	else if (response->result() == Plugin::Common_ResultCode_CRITCAL && result != Plugin::Common_ResultCode_CRITCAL)
		response->set_result(result);
	else if (response->result() == Plugin::Common_ResultCode_CRITCAL)
		return;
	else if (response->result() == Plugin::Common_ResultCode_UNKNOWN && result != Plugin::Common_ResultCode_UNKNOWN)
		response->set_result(result);
	else if (response->result() == Plugin::Common_ResultCode_UNKNOWN)
		return;
}

void CheckHelpers::check_critical(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_simple_status(Plugin::Common_ResultCode_CRITCAL, request, response);
}
void CheckHelpers::check_warning(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_simple_status(Plugin::Common_ResultCode_WARNING, request, response);
}
void CheckHelpers::check_ok(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_simple_status(Plugin::Common_ResultCode_OK, request, response);
}
void CheckHelpers::check_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	nscapi::protobuf::functions::set_response_good(*response, utf8::cvt<std::string>(get_core()->getApplicationVersionString()));
}

bool simple_query(const std::string &command, const std::vector<std::string> &arguments, Plugin::QueryResponseMessage::Response *response) {
	std::string local_response_buffer;
	if (nscapi::core_helper::simple_query(command, arguments, local_response_buffer) != NSCAPI::isSuccess) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute: " + command);
		return false;
	}
	Plugin::QueryResponseMessage local_response;
	local_response.ParseFromString(local_response_buffer);
	if (local_response.payload_size() != 1) {
		nscapi::protobuf::functions::set_response_bad(*response, "Invalid payload size: " + command);
		return false;
	}
	response->CopyFrom(local_response.payload(0));
	return true;
}

void CheckHelpers::check_change_status(::Plugin::Common_ResultCode status, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response)  {
	po::options_description desc = nscapi::program_options::create_desc(request);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	if (request.arguments_size() == 0)
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	std::string command = request.arguments(0);
	std::vector<std::string> arguments;
	for (int i=1;i<request.arguments_size();i++)
		arguments.push_back(request.arguments(i));
	Plugin::QueryResponseMessage::Response local_response;
	if (!simple_query(command, arguments, &local_response))
		return;
	response->CopyFrom(local_response);
	response->set_result(status);
}
void CheckHelpers::check_always_critical(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_change_status(Plugin::Common_ResultCode_CRITCAL, request, response);
}
void CheckHelpers::check_always_warning(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_change_status(Plugin::Common_ResultCode_WARNING, request, response);
}
void CheckHelpers::check_always_ok(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_change_status(Plugin::Common_ResultCode_OK, request, response);
}
void CheckHelpers::check_negate(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response)  {
	std::string command;
	std::vector<std::string> arguments;
	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("ok,o",		po::value<std::string>(), "The state to return instead of OK")
		("warning,w",	po::value<std::string>(), "The state to return instead of WARNING")
		("critical,c",	po::value<std::string>(), "The state to return instead of CRITICAL")
		("unknown,u",	po::value<std::string>(), "The state to return instead of UNKNOWN")

		("command,q",	po::value<std::string>(&command), "Wrapped command to execute")
		("arguments,a",	po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		;
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	if (command.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	Plugin::QueryResponseMessage::Response local_response;
	if (!simple_query(command, arguments, &local_response))
		return;
	response->CopyFrom(local_response);
	::Plugin::Common_ResultCode new_o =  ::Plugin::Common_ResultCode_OK;
	::Plugin::Common_ResultCode new_w =  ::Plugin::Common_ResultCode_WARNING;
	::Plugin::Common_ResultCode new_c =  ::Plugin::Common_ResultCode_CRITCAL;
	::Plugin::Common_ResultCode new_u =  ::Plugin::Common_ResultCode_UNKNOWN;
	
	if (vm.count("ok"))
		new_o = nscapi::protobuf::functions::parse_nagios(vm["ok"].as<std::string>());
	if (vm.count("warning"))
		new_w = nscapi::protobuf::functions::parse_nagios(vm["warning"].as<std::string>());
	if (vm.count("critical"))
		new_c = nscapi::protobuf::functions::parse_nagios(vm["critical"].as<std::string>());
	if (vm.count("unknown"))
		new_u = nscapi::protobuf::functions::parse_nagios(vm["unknown"].as<std::string>());
	if (response->result() == Plugin::Common_ResultCode_OK)
		response->set_result(new_o);
	if (response->result() == Plugin::Common_ResultCode_WARNING)
		response->set_result(new_w);
	if (response->result() == Plugin::Common_ResultCode_CRITCAL)
		response->set_result(new_c);
	if (response->result() == Plugin::Common_ResultCode_UNKNOWN)
		response->set_result(new_u);
}

void CheckHelpers::check_multi(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	po::options_description desc = nscapi::program_options::create_desc(request);
	std::vector<std::string> arguments;
	std::string separator;
	std::string prefix;
	std::string suffix;
	desc.add_options()
		("command",	po::value<std::vector<std::string> >(&arguments), "Commands to run (can be used multiple times)")
		("arguments",	po::value<std::vector<std::string> >(&arguments), "Deprecated alias for command")
		("separator",	po::value<std::string>(&separator)->default_value(", "), "Separator between messages")
		("prefix",	po::value<std::string>(&prefix), "Message prefix")
		("suffix",	po::value<std::string>(&suffix), "Message suffix")
		;
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	if (arguments.size() == 0)
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	std::string message;
	Plugin::Common_ResultCode result = Plugin::Common_ResultCode_OK;

	BOOST_FOREACH(std::string arg, arguments) {
		std::list<std::string> tmp = strEx::s::splitEx(arg, std::string(" "));
		if (tmp.empty()) {
			return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
		}
		std::string command = tmp.front(); 
		std::vector<std::string> args(++(tmp.begin()), tmp.end());
		Plugin::QueryResponseMessage::Response local_response;
		if (!simple_query(command, args, &local_response)) 
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute command: " + command);
		for (int j=0;j<local_response.perf_size();j++) {
			response->add_perf()->CopyFrom(local_response.perf(j));
		}
		escalate_result(response, local_response.result());
		if (!message.empty())
			message += separator;
		message += local_response.message();
	}
	response->set_message(prefix + message + suffix);
}

struct worker_object {
	void proc(std::string command, std::vector<std::string> arguments) {
		nscapi::core_helper::simple_query(command, arguments, response_buffer);
	}
	NSCAPI::nagiosReturn ret;
	std::string response_buffer;
};

void CheckHelpers::check_timeout(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response)  {
	std::string command;
	std::vector<std::string> arguments;
	unsigned long timeout = 30;
	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("timeout,t",	po::value<unsigned long>(&timeout), "The timeout value")
		("command,q",	po::value<std::string>(&command), "Wrapped command to execute")
		("arguments,a",	po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		("return,r",	po::value<std::string>(), "The return status")
		;
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	if (command.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	::Plugin::Common_ResultCode ret =  ::Plugin::Common_ResultCode_CRITCAL;
	if (vm.count("return"))
		ret = nscapi::protobuf::functions::parse_nagios(vm["return"].as<std::string>());

	worker_object obj;
	boost::shared_ptr<boost::thread> t = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&worker_object::proc, obj, command, arguments)));

	if (t->timed_join(boost::posix_time::seconds(timeout))) {
		if (obj.ret != NSCAPI::isSuccess) {
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute: " + command);
		}
		Plugin::QueryResponseMessage local_response;
		local_response.ParseFromString(obj.response_buffer);
		if (local_response.payload_size() != 1) {
			return nscapi::protobuf::functions::set_response_bad(*response, "Invalid payload size: " + command);
		}
		response->CopyFrom(local_response.payload(0));
	} else {
		t->detach();
		nscapi::protobuf::functions::set_response_bad(*response, "Thread failed to return within given timeout");
	}
}

struct normal_sort {
	bool operator() (const ::Plugin::Common::PerformanceData &v1, const ::Plugin::Common::PerformanceData &v2) const {
		if (v1.type() != ::Plugin::Common_DataType_INT)
			return false;
		if (v2.type() != ::Plugin::Common_DataType_INT)
			return false;
		if (v1.int_value().value() > v2.int_value().value())
			return true;
		return false;
	}
};
struct reverse_sort {
	bool operator() (const ::Plugin::Common::PerformanceData &v1, const ::Plugin::Common::PerformanceData &v2) const {
		if (v1.type() != ::Plugin::Common_DataType_INT)
			return false;
		if (v2.type() != ::Plugin::Common_DataType_INT)
			return false;
		if (v1.int_value().value() < v2.int_value().value())
			return true;
		return false;
	}
};

void CheckHelpers::filter_perf(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response)  {
	std::string command, sort;
	int limit;
	std::vector<std::string> arguments;
	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("sort",	po::value<std::string>(&sort)->default_value("none"), "The sort order to use: none, normal or reversed")
		("limit",	po::value<int>(&limit)->default_value(0), "The maximum number of items to return (0 returns all items)")
		("command",	po::value<std::string>(&command), "Wrapped command to execute")
		("arguments",	po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		;

	po::positional_options_description p;
	p.add("arguments", -1);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, p)) 
		return;
	if (command.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	simple_query(command, arguments, response);

	std::vector<::Plugin::Common::PerformanceData> perf;
	for (int i=0;i<response->perf_size(); i++) {
		perf.push_back(response->perf(i));
	}
	if (sort == "normal")
		std::sort(perf.begin(), perf.end(), normal_sort());
	else if (sort == "reverse")
		std::sort(perf.begin(), perf.end(), reverse_sort());
	response->clear_perf();
	if (limit > 0 && perf.size() > limit)
		 perf.erase(perf.begin()+limit, perf.end());
	BOOST_FOREACH(const ::Plugin::Common::PerformanceData &v, perf) {
		response->add_perf()->CopyFrom(v);
	}
}

