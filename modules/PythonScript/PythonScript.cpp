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

#include "PythonScript.h"
#include "script_wrapper.hpp"
#include "python_script.hpp"
#include "script_provider.hpp"
#include "extscr_cli.h"

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/macros.hpp>

#include <boost/python.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>


namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;
namespace py = boost::python;

bool PythonScript::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	alias_ = alias;

	if (mode == NSCAPI::reloadStart) {
		nscapi::core_helper ch(get_core(), get_id());
		BOOST_FOREACH(const std::string &s, script_wrapper::functions::get()->get_commands()) {
			ch.unregister_command(s);
		}
		if (provider_) {
			provider_->clear();
		}
	}

	try {
		root_ = get_base_path();

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "python");

		provider_.reset(new script_provider(get_id(), get_core(), settings.alias().get_path(), root_));

		settings.alias().add_path_to_settings()

			("scripts", sh::fun_values_path(boost::bind(&PythonScript::loadScript, this, _1, _2)),
				"Python scripts", "A list of scripts available to run from the PythonScript module.",
				"SCRIPT", "For more configuration options add a dedicated section")
			;

		settings.alias().add_templates()
			("scripts", "plus", "Add a simple script",
				"Add binding for a simple script",
				"{"
				"\"fields\": [ "
				" { \"id\": \"alias\",		\"title\" : \"Alias\",		\"type\" : \"input\",		\"desc\" : \"This has to be unique and if you load a script twice the script can use the alias to diferentiate between instances.\"} , "
				" { \"id\": \"script\",		\"title\" : \"Script\",		\"type\" : \"data-choice\",	\"desc\" : \"The name of the script\",\"exec\" : \"PythonScript list --json\" } , "
				" { \"id\": \"cmd\",		\"key\" : \"command\", \"title\" : \"A\",	\"type\" : \"hidden\",		\"desc\" : \"A\" } "
				" ], "
				"\"events\": { "
				"\"onSave\": \"(function (node) { node.save_path = self.path; var f = node.get_field('cmd'); f.key = node.get_field('alias').value(); f.value(node.get_field('script').value()); })\""
				"}"
				"}")
			;


		settings.register_all();

		python_script::init();

		settings.notify();
	} catch (...) {
		NSC_LOG_ERROR_STD("Exception caught: <UNKNOWN EXCEPTION>");
		return false;
	}
	return true;
}

void PythonScript::loadScript(std::string alias, std::string file) {
	if (!provider_) {
		NSC_LOG_ERROR_STD("Could not find script: no provider " + file);
	} else {
		provider_->add_command(alias, file, alias_);
	}
}

bool PythonScript::unloadModule() {
	if (provider_) {
		provider_->clear();
	}
	return true;
}

bool PythonScript::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	std::string command = request.command();
	if (command == "ext-scr" && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module)
		command = "help";
	try {
		if (command == "help") {
			nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp py [add|execute|list|install|delete] --help");
			return true;
		} else if (command == "execute" || command == "python-script") {
			execute_script(request, response);
			return true;
		}
		extscr_cli client(provider_, alias_);
		if (client.run(command, request, response)) {
			return true;
		}
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: ");
	}


	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_cmdline(request.command())) {
		std::string buffer;
		inst->handle_exec(request.command(), request_message.SerializeAsString(), buffer);
		Plugin::ExecuteResponseMessage local_response;
		local_response.ParseFromString(buffer);
		if (local_response.payload_size() != 1) {
			nscapi::protobuf::functions::set_response_bad(*response, "Invalid response: " + request.command());
			return true;
		}
		response->CopyFrom(local_response.payload(0));
	}
	if (inst->has_simple_cmdline(request.command())) {
		std::list<std::string> args;
		for (int i = 0; i < request.arguments_size(); i++)
			args.push_back(request.arguments(i));
		std::string result;
		NSCAPI::nagiosReturn ret = inst->handle_simple_exec(request.command(), args, result);
		response->set_message(result);
		response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
		return true;
	}
	return false;
}

void PythonScript::execute_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::options_description desc = nscapi::program_options::create_desc(request);
	po::variables_map vm;
	nscapi::program_options::unrecognized_map script_options;
	std::string file;
	desc.add_options()
		("help", "Show help.")
		("script", po::value<std::string>(&file), "The script to run")
		("file", po::value<std::string>(&file), "The script to run")
		;


	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.allow_unregistered().run();
		po::store(parsed, vm);
		po::notify(vm);
		script_options = po::collect_unrecognized(parsed.options, po::include_positional);
		if (!script_options.empty() && (script_options[0] == "execute" || script_options[0] == "exec" || script_options[0] == "python-script"))
			script_options.erase(script_options.begin());

	} catch (const std::exception &e) {
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
		return;
	}

	boost::optional<boost::filesystem::path> ofile = provider_->find_file(file);
	if (!ofile) {
		nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file);
		return;
	}
	std::string script_file = ofile->string();
	python_script script(get_id(), root_.string(), "", "", script_file);
	std::list<std::string> ops(script_options.begin(), script_options.end());
	if (!script.callFunction("__main__", ops)) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute __main__");
		return;
	}
	nscapi::protobuf::functions::set_response_good(*response, "");
}


void PythonScript::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_function(request.command())) {
		std::string buffer;
		if (inst->handle_query(request.command(), request_message.SerializeAsString(), buffer) != NSCAPI::query_return_codes::returnOK) {
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute script " + request.command());
		}
		Plugin::QueryResponseMessage local_response;
		local_response.ParseFromString(buffer);
		if (local_response.payload_size() != 1)
			return nscapi::protobuf::functions::set_response_bad(*response, "Invalid response: " + request.command());
		response->CopyFrom(local_response.payload(0));
	}
	if (inst->has_simple(request.command())) {
		std::list<std::string> args;
		for (int i = 0; i < request.arguments_size(); i++)
			args.push_back(request.arguments(i));
		std::string msg, perf;
		NSCAPI::nagiosReturn ret = inst->handle_simple_query(request.command(), args, msg, perf);
		::Plugin::QueryResponseMessage_Response_Line *line = response->add_lines();
		nscapi::protobuf::functions::parse_performance_data(line, perf);
		line->set_message(msg);
		response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
	}
}

void PythonScript::handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_message_handler(channel)) {
		std::string buffer;
		if (inst->handle_message(channel, request_message.SerializeAsString(), buffer) == NSCAPI::api_return_codes::isSuccess) {
			Plugin::SubmitResponseMessage local_response;
			local_response.ParseFromString(buffer);
			if (local_response.payload_size() == 1) {
				response->CopyFrom(local_response.payload(0));
				return;
			}
		}
	}
	if (inst->has_simple_message_handler(channel)) {
		BOOST_FOREACH(::Plugin::QueryResponseMessage_Response_Line line, request.lines()) {
			std::string perf = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
			if (inst->handle_simple_message(channel, request.source(), request.command(), request.result(), line.message(), perf) != NSCAPI::api_return_codes::isSuccess)
				return nscapi::protobuf::functions::set_response_bad(*response, "Invalid response: " + channel);
		}
		return nscapi::protobuf::functions::set_response_good(*response, "");
	}
	return nscapi::protobuf::functions::set_response_bad(*response, "Unable to process message: " + channel);
}

void PythonScript::onEvent(const Plugin::EventMessage &request, const std::string &buffer) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_event_handler("$$event$$")) {
		inst->on_event("$$event$$", buffer);
	}
	BOOST_FOREACH(const ::Plugin::EventMessage::Request &line, request.payload()) {
		if (inst->has_simple_event_handler(line.event())) {
			boost::python::dict data;
			BOOST_FOREACH(const ::Plugin::Common::KeyValue e, line.data()) {
				data[e.key()] = e.value();
			}
			inst->on_simple_event(line.event(), data);
		}
	}
}

void PythonScript::submitMetrics(const Plugin::MetricsMessage &response) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_submit_metrics()) {
		std::string buffer;
		inst->submit_metrics(response.SerializeAsString());
	}
}
void PythonScript::fetchMetrics(Plugin::MetricsMessage::Response *response) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_metrics_fetcher()) {
		std::string buffer;
		Plugin::MetricsMessage::Response r2;
		inst->fetch_metrics(buffer);
		r2.ParseFromString(buffer);
		BOOST_FOREACH(const ::Plugin::Common_MetricsBundle &b, r2.bundles())
			response->add_bundles()->CopyFrom(b);
	}
}
