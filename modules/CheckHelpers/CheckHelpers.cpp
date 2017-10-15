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

#include "CheckHelpers.h"

#include <parsers/filter/cli_helper.hpp>

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

#include <str/utils.hpp>

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>

#include <vector>
#include <algorithm>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

void check_simple_status(::Plugin::Common_ResultCode status, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	po::options_description desc = nscapi::program_options::create_desc(request);
	std::string msg;
	desc.add_options()
		("message", po::value<std::string>(&msg)->default_value("No message"), "Message to return")
		;
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	response->set_result(status);
	response->add_lines()->set_message(msg);
}

void escalate_result(Plugin::QueryResponseMessage::Response * response, ::Plugin::Common_ResultCode new_result) {
	::Plugin::Common_ResultCode current = response->result();
	if (current == new_result)
		return;
	else if (current == Plugin::Common_ResultCode_OK && new_result != Plugin::Common_ResultCode_OK)
		response->set_result(new_result);
	else if (new_result == Plugin::Common_ResultCode_OK)
		return;
	else if (current == Plugin::Common_ResultCode_WARNING && new_result != Plugin::Common_ResultCode_WARNING)
		response->set_result(new_result);
	else if (new_result == Plugin::Common_ResultCode_WARNING)
		return;
	else if (current == Plugin::Common_ResultCode_CRITICAL && new_result != Plugin::Common_ResultCode_CRITICAL)
		response->set_result(new_result);
	else if (new_result == Plugin::Common_ResultCode_CRITICAL)
		return;
	else if (current == Plugin::Common_ResultCode_UNKNOWN && new_result != Plugin::Common_ResultCode_UNKNOWN)
		response->set_result(new_result);
}

void CheckHelpers::check_critical(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_simple_status(Plugin::Common_ResultCode_CRITICAL, request, response);
}
void CheckHelpers::check_warning(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_simple_status(Plugin::Common_ResultCode_WARNING, request, response);
}
void CheckHelpers::check_ok(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_simple_status(Plugin::Common_ResultCode_OK, request, response);
}
void CheckHelpers::check_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	po::options_description desc = nscapi::program_options::create_desc(request);
	std::string msg;
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	nscapi::protobuf::functions::set_response_good(*response, utf8::cvt<std::string>(get_core()->getApplicationVersionString()));
}

bool CheckHelpers::simple_query(const std::string &command, const std::vector<std::string> &arguments, Plugin::QueryResponseMessage::Response *response) {
	std::string local_response_buffer;
	nscapi::core_helper ch(get_core(), get_id());
	if (ch.simple_query(command, arguments, local_response_buffer) != NSCAPI::api_return_codes::isSuccess) {
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

bool CheckHelpers::simple_query(const std::string &command, const std::list<std::string> &arguments, Plugin::QueryResponseMessage::Response *response) {
	std::string local_response_buffer;
	nscapi::core_helper ch(get_core(), get_id());
	if (ch.simple_query(command, arguments, local_response_buffer) != NSCAPI::api_return_codes::isSuccess) {
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

void CheckHelpers::check_change_status(::Plugin::Common_ResultCode status, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	po::options_description desc = nscapi::program_options::create_desc(request);
	po::variables_map vm;
	std::vector<std::string> args;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, true, args))
		return;
	if (args.size() == 0)
		return nscapi::protobuf::functions::set_response_bad(*response, "Needs at least one command");
	std::string command = args.front();
	std::vector<std::string> arguments(args.begin() + 1, args.end());
	Plugin::QueryResponseMessage::Response local_response;
	if (!simple_query(command, arguments, &local_response))
		status = ::Plugin::Common_ResultCode_UNKNOWN;
	response->CopyFrom(local_response);
	response->set_result(status);
}
void CheckHelpers::check_always_critical(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_change_status(Plugin::Common_ResultCode_CRITICAL, request, response);
}
void CheckHelpers::check_always_warning(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_change_status(Plugin::Common_ResultCode_WARNING, request, response);
}
void CheckHelpers::check_always_ok(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_change_status(Plugin::Common_ResultCode_OK, request, response);
}
void CheckHelpers::check_negate(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	std::string command;
	std::vector<std::string> arguments;
	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("ok,o", po::value<std::string>(), "The state to return instead of OK")
		("warning,w", po::value<std::string>(), "The state to return instead of WARNING")
		("critical,c", po::value<std::string>(), "The state to return instead of CRITICAL")
		("unknown,u", po::value<std::string>(), "The state to return instead of UNKNOWN")

		("command,q", po::value<std::string>(&command), "Wrapped command to execute")
		("arguments,a", po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
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
	::Plugin::Common_ResultCode new_o = ::Plugin::Common_ResultCode_OK;
	::Plugin::Common_ResultCode new_w = ::Plugin::Common_ResultCode_WARNING;
	::Plugin::Common_ResultCode new_c = ::Plugin::Common_ResultCode_CRITICAL;
	::Plugin::Common_ResultCode new_u = ::Plugin::Common_ResultCode_UNKNOWN;

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
	if (response->result() == Plugin::Common_ResultCode_CRITICAL)
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
		("command", po::value<std::vector<std::string> >(&arguments), "Commands to run (can be used multiple times)")
		("arguments", po::value<std::vector<std::string> >(&arguments), "Deprecated alias for command")
		("separator", po::value<std::string>(&separator)->default_value(", "), "Separator between messages")
		("prefix", po::value<std::string>(&prefix), "Message prefix")
		("suffix", po::value<std::string>(&suffix), "Message suffix")
		;
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	if (arguments.size() == 0)
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	response->set_result(Plugin::Common_ResultCode_OK);
	BOOST_FOREACH(std::string command_line, arguments) {
		std::list<std::string> args;
		str::utils::parse_command(command_line, args);

		if (args.size() == 0) {
			return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
		}
		std::string command = args.front(); args.pop_front();
		Plugin::QueryResponseMessage::Response local_response;
		if (!simple_query(command, args, &local_response))
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute command: " + command);
		bool first = true;
		BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response_Line &line, local_response.lines()) {
			if (first && response->lines_size() > 0) {
				::Plugin::QueryResponseMessage_Response_Line *nLine = response->add_lines();
				nLine->CopyFrom(line);
				nLine->set_message(separator + nLine->message());
				first = false;
			} else {
				response->add_lines()->CopyFrom(line);
			}
		}
		escalate_result(response, local_response.result());
	}
	if (response->lines_size() > 0) {
		if (!prefix.empty())
			response->mutable_lines(0)->set_message(prefix + response->lines(0).message());
		if (!suffix.empty())
			response->mutable_lines(response->lines_size() - 1)->set_message(response->lines(response->lines_size() - 1).message() + suffix);
	}
}


void CheckHelpers::check_and_forward(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	po::options_description desc = nscapi::program_options::create_desc(request);
	std::vector<std::string> arguments;
	std::string target;
	std::string command;
	desc.add_options()
		("target", po::value<std::string>(&target), "Commands to run (can be used multiple times)")
		("command", po::value<std::string>(&command), "Commands to run (can be used multiple times)")
		("arguments", po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		;
	po::variables_map vm;
	std::vector<std::string> args;

	po::positional_options_description p;
	p.add("arguments", -1);

	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, p))
		return;
	if (command.size() == 0)
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);


	std::string local_response;
	nscapi::core_helper ch(get_core(), get_id());
	if (ch.simple_query(command, arguments, local_response) != NSCAPI::api_return_codes::isSuccess) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute: " + command);
		return;
	}

	std::string submit_response;
	if (!get_core()->submit_message(target, local_response, submit_response)) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to submit to: " + target);
	}
	nscapi::protobuf::functions::set_response_good(*response, "Message submitted: " + target);
}

struct worker_object {
	void proc(nscapi::core_wrapper *core, int plugin_id, std::string command, std::vector<std::string> arguments) {
		nscapi::core_helper ch(core, plugin_id);
		ret = ch.simple_query(command, arguments, response_buffer);
	}
	NSCAPI::nagiosReturn ret;
	std::string response_buffer;
};

void CheckHelpers::check_timeout(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	std::string command;
	std::vector<std::string> arguments;
	unsigned long timeout = 30;
	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("timeout,t", po::value<unsigned long>(&timeout), "The timeout value")
		("command,q", po::value<std::string>(&command), "Wrapped command to execute")
		("arguments,a", po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		("return,r", po::value<std::string>(), "The return status")
		;
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	if (command.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);

	worker_object obj;
	boost::shared_ptr<boost::thread> t = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&worker_object::proc, obj, get_core(), get_id(), command, arguments)));

	if (t->timed_join(boost::posix_time::seconds(timeout))) {
		if (obj.ret != NSCAPI::query_return_codes::returnOK) {
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute: " + command);
		}
		Plugin::QueryResponseMessage local_response;
		local_response.ParseFromString(obj.response_buffer);
		if (local_response.payload_size() != 1) {
			return nscapi::protobuf::functions::set_response_bad(*response, "Invalid payload size: " + command);
		}
		response->CopyFrom(local_response.payload(0));
		if (vm.count("return"))
			response->set_result(nscapi::protobuf::functions::parse_nagios(vm["return"].as<std::string>()));
	} else {
		t->detach();
		nscapi::protobuf::functions::set_response_bad(*response, "Thread failed to return within given timeout");
	}
}

struct normal_sort {
	bool operator() (const ::Plugin::Common::PerformanceData &v1, const ::Plugin::Common::PerformanceData &v2) const {
		if (!v1.has_int_value())
			return false;
		if (!v2.has_int_value())
			return false;
		if (v1.int_value().value() > v2.int_value().value())
			return true;
		return false;
	}
};
struct reverse_sort {
	bool operator() (const ::Plugin::Common::PerformanceData &v1, const ::Plugin::Common::PerformanceData &v2) const {
		if (!v1.has_int_value())
			return false;
		if (!v2.has_int_value())
			return false;
		if (v1.int_value().value() < v2.int_value().value())
			return true;
		return false;
	}
};

void CheckHelpers::filter_perf(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	std::string command, sort;
	std::size_t limit;
	std::vector<std::string> arguments;
	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("sort", po::value<std::string>(&sort)->default_value("none"), "The sort order to use: none, normal or reversed")
		("limit", po::value<std::size_t>(&limit)->default_value(0), "The maximum number of items to return (0 returns all items)")
		("command", po::value<std::string>(&command), "Wrapped command to execute")
		("arguments", po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		;

	po::positional_options_description p;
	p.add("arguments", -1);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, p))
		return;
	if (command.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	simple_query(command, arguments, response);
	std::vector<Plugin::Common::PerformanceData> perfs;

	std::stringstream ss;
	BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response_Line &line, response->lines()) {
		ss << line.message() << "\n";
		BOOST_FOREACH(const ::Plugin::Common_PerformanceData &perf, line.perf()) {
			perfs.push_back(perf);
		}
	}

	if (sort == "normal")
		std::sort(perfs.begin(), perfs.end(), normal_sort());
	else if (sort == "reverse")
		std::sort(perfs.begin(), perfs.end(), reverse_sort());
	response->clear_lines();
	::Plugin::QueryResponseMessage_Response_Line* line = response->add_lines();
	line->set_message(ss.str());
	if (limit > 0 && perfs.size() > limit)
		perfs.erase(perfs.begin() + limit, perfs.end());
	BOOST_FOREACH(const ::Plugin::Common::PerformanceData &v, perfs) {
		line->add_perf()->CopyFrom(v);
	}
}

#include <parsers/where.hpp>
#include <parsers/where/helpers.hpp>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace perf_filter {
	struct filter_obj {
		const ::Plugin::Common_PerformanceData& data;

		filter_obj(const ::Plugin::Common_PerformanceData& data) : data(data) {}

		std::string get_key() const {
			return data.alias();
		}
		std::string get_unit() const {
			if (data.has_bool_value())
				return data.bool_value().unit();
			if (data.has_float_value())
				return data.float_value().unit();
			if (data.has_int_value())
				return data.int_value().unit();
			return "";
		}
		std::string get_value() const {
			if (data.has_bool_value())
				return str::xtos(data.bool_value().value());
			if (data.has_float_value())
				return str::xtos(data.float_value().value());
			if (data.has_int_value())
				return str::xtos(data.int_value().value());
			if (data.has_string_value())
				return data.string_value().value();
			return "";
		}
		std::string get_warn() const {
			if (data.has_bool_value())
				return str::xtos(data.bool_value().warning());
			if (data.has_float_value())
				return str::xtos(data.float_value().warning());
			if (data.has_int_value())
				return str::xtos(data.int_value().warning());
			return "";
		}
		std::string get_crit() const {
			if (data.has_bool_value())
				return str::xtos(data.bool_value().critical());
			if (data.has_float_value())
				return str::xtos(data.float_value().critical());
			if (data.has_int_value())
				return str::xtos(data.int_value().critical());
			return "";
		}
		std::string get_max() const {
			if (data.has_float_value())
				return str::xtos(data.float_value().maximum());
			if (data.has_int_value())
				return str::xtos(data.int_value().maximum());
			return "";
		}
		std::string get_min() const {
			if (data.has_float_value())
				return str::xtos(data.float_value().minimum());
			if (data.has_int_value())
				return str::xtos(data.int_value().minimum());
			return "";
		}
	};
	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};

	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

	filter_obj_handler::filter_obj_handler() {
		registry_.add_string()
			("key", boost::bind(&filter_obj::get_key, _1), "Major version number")
			("value", boost::bind(&filter_obj::get_value, _1), "Major version number")
			("unit", boost::bind(&filter_obj::get_unit, _1), "Major version number")
			("warn", boost::bind(&filter_obj::get_warn, _1), "Major version number")
			("crit", boost::bind(&filter_obj::get_crit, _1), "Major version number")
			("max", boost::bind(&filter_obj::get_min, _1), "Major version number")
			("min", boost::bind(&filter_obj::get_max, _1), "Major version number")
			("message", boost::bind(&filter_obj::get_key, _1), "Major version number")
			;
	}
}

void CheckHelpers::render_perf(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;
	modern_filter::cli_helper<perf_filter::filter> filter_helper(request, response, data);

	std::string command;
	std::vector<std::string> arguments;
	bool remove_perf = false;
	po::options_description desc = nscapi::program_options::create_desc(request);
	perf_filter::filter filter;
	filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
	filter_helper.add_syntax("%(status): %(message) %(list)", "%(key)\t%(value)\t%(unit)\t%(warn)\t%(crit)\t%(min)\t%(max)\n", "%(key)", "", "");
	filter_helper.get_desc().add_options()
		("command", po::value<std::string>(&command), "Wrapped command to execute")
		("arguments", po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		("remove-perf", po::bool_switch(&remove_perf), "List of arguments (for wrapped command)")
		;

	po::positional_options_description p;
	p.add("arguments", -1);

	if (!filter_helper.parse_options(p))
		return;

	if (!filter_helper.build_filter(filter))
		return;

	if (command.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	simple_query(command, arguments, response);
	for (int i = 0; i < response->lines_size(); i++) {
		::Plugin::QueryResponseMessage_Response_Line* line = response->mutable_lines(i);
		BOOST_FOREACH(const ::Plugin::Common_PerformanceData &perf, line->perf()) {
			boost::shared_ptr<perf_filter::filter_obj> record(new perf_filter::filter_obj(perf));
			filter.match(record);
		}
		if (remove_perf)
			line->clear_perf();
	}
	filter_helper.post_process(filter);
}

void CheckHelpers::xform_perf(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;

	std::string command, mode, field, replace;
	std::vector<std::string> arguments;
	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("command", po::value<std::string>(&command), "Wrapped command to execute")
		("arguments", po::value<std::vector<std::string> >(&arguments), "List of arguments (for wrapped command)")
		("mode", po::value<std::string>(&mode), "Transformation mode: extract to fetch data or minmax to add missing min/max")
		("field", po::value<std::string>(&field), "Field to work with (value, warn, crit, max, min)")
		("replace", po::value<std::string>(&replace), "Replace expression for the alias")
		;

	po::positional_options_description p;
	p.add("arguments", -1);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, p))
		return;

	if (command.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing command", *response);
	simple_query(command, arguments, response);


	if (mode == "extract") {

		std::vector<std::string> repl;
		boost::split(repl, replace, boost::is_any_of("="));
		if (repl.size() != 2)
			return nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid syntax replace string", *response);

		for (int i = 0; i < response->lines_size(); i++) {
			::Plugin::QueryResponseMessage_Response_Line* line = response->mutable_lines(i);
			std::vector<Plugin::Common::PerformanceData> perf;
			for (int i = 0; i < line->perf_size(); i++) {
				const Plugin::Common::PerformanceData &cp = line->perf(i);
				Plugin::Common::PerformanceData np;
				np.CopyFrom(cp);
				np.set_alias(boost::replace_all_copy(cp.alias(), repl[0], repl[1]));
				if (field == "max") {
					if (cp.has_int_value()) {
						np.mutable_int_value()->set_value(cp.int_value().maximum());
						perf.push_back(np);
					} else if (cp.has_float_value()) {
						np.mutable_float_value()->set_value(cp.float_value().maximum());
						perf.push_back(np);
					}
				} else if (field == "min") {
					if (cp.has_int_value()) {
						np.mutable_int_value()->set_value(cp.int_value().minimum());
						perf.push_back(np);
					} else if (cp.has_float_value()) {
						np.mutable_float_value()->set_value(cp.float_value().minimum());
						perf.push_back(np);
					}
				}
			}
			BOOST_FOREACH(const Plugin::Common::PerformanceData &p, perf) {
				line->add_perf()->CopyFrom(p);
			}
		}
	} else if (mode == "minmax") {
		for (int i = 0; i < response->lines_size(); i++) {
			::Plugin::QueryResponseMessage_Response_Line* line = response->mutable_lines(i);
			for (int i=0;i<line->perf_size();i++) {
				Plugin::Common_PerformanceData *np = line->mutable_perf(i);
				if (np->has_int_value()) {
					if (np->int_value().unit() == "%") {
						np->mutable_int_value()->set_maximum(100);
						np->mutable_int_value()->set_minimum(0);
					}
				} else if (np->has_float_value()) {
					if (np->float_value().unit() == "%") {
						np->mutable_float_value()->set_maximum(100);
						np->mutable_float_value()->set_minimum(0);
					}
				}
			}
		}
	} else {
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid mode specified", *response);
	}
}