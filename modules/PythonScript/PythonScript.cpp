/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PythonScript.h"
#include <time.h>
#include <error.hpp>

#include <boost/python.hpp>
#include <boost/program_options.hpp>

#include <json_spirit.h>


#include <strEx.h>
#include <file_helpers.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/macros.hpp>

#include "script_wrapper.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <nscapi/nscapi_settings_helper.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;
using namespace boost::python;

BOOST_PYTHON_MODULE(NSCP) {
	class_<script_wrapper::settings_wrapper, boost::shared_ptr<script_wrapper::settings_wrapper> >("Settings", no_init)
		.def("get", &script_wrapper::settings_wrapper::create)
		.staticmethod("get")
		.def("create", &script_wrapper::settings_wrapper::create)
		.staticmethod("create")
		.def("get_section", &script_wrapper::settings_wrapper::get_section)
		.def("get_string", &script_wrapper::settings_wrapper::get_string)
		.def("set_string", &script_wrapper::settings_wrapper::set_string)
		.def("get_bool", &script_wrapper::settings_wrapper::get_bool)
		.def("set_bool", &script_wrapper::settings_wrapper::set_bool)
		.def("get_int", &script_wrapper::settings_wrapper::get_int)
		.def("set_int", &script_wrapper::settings_wrapper::set_int)
		.def("save", &script_wrapper::settings_wrapper::save)
		.def("register_path", &script_wrapper::settings_wrapper::settings_register_path)
		.def("register_key", &script_wrapper::settings_wrapper::settings_register_key)
		.def("query", &script_wrapper::settings_wrapper::query)
		;
	class_<script_wrapper::function_wrapper, boost::shared_ptr<script_wrapper::function_wrapper> >("Registry", no_init)
		.def("get", &script_wrapper::function_wrapper::create)
		.staticmethod("get")
		.def("create", &script_wrapper::function_wrapper::create)
		.staticmethod("create")
		.def("function", &script_wrapper::function_wrapper::register_function)
		.def("simple_function", &script_wrapper::function_wrapper::register_simple_function)
		.def("cmdline", &script_wrapper::function_wrapper::register_cmdline)
		.def("simple_cmdline", &script_wrapper::function_wrapper::register_simple_cmdline)
		.def("subscription", &script_wrapper::function_wrapper::subscribe_function)
		.def("simple_subscription", &script_wrapper::function_wrapper::subscribe_simple_function)
		.def("submit_metrics", &script_wrapper::function_wrapper::register_submit_metrics)
		.def("fetch_metrics", &script_wrapper::function_wrapper::register_fetch_metrics)
		.def("event_pb", &script_wrapper::function_wrapper::register_event_pb)
		.def("event", &script_wrapper::function_wrapper::register_event)
		.def("query", &script_wrapper::function_wrapper::query)
		;
	class_<script_wrapper::command_wrapper, boost::shared_ptr<script_wrapper::command_wrapper> >("Core", no_init)
		.def("get", &script_wrapper::command_wrapper::create)
		.staticmethod("get")
		.def("create", &script_wrapper::command_wrapper::create)
		.staticmethod("create")
		.def("simple_query", &script_wrapper::command_wrapper::simple_query)
		.def("query", &script_wrapper::command_wrapper::query)
		.def("simple_exec", &script_wrapper::command_wrapper::simple_exec)
		.def("exec", &script_wrapper::command_wrapper::exec)
		.def("simple_submit", &script_wrapper::command_wrapper::simple_submit)
		.def("submit", &script_wrapper::command_wrapper::submit)
		.def("reload", &script_wrapper::command_wrapper::reload)
		.def("load_module", &script_wrapper::command_wrapper::load_module)
		.def("unload_module", &script_wrapper::command_wrapper::unload_module)
		.def("expand_path", &script_wrapper::command_wrapper::expand_path)
		;

	enum_<script_wrapper::status>("status")
		.value("CRITICAL", script_wrapper::CRIT)
		.value("WARNING", script_wrapper::WARN)
		.value("UNKNOWN", script_wrapper::UNKNOWN)
		.value("OK", script_wrapper::OK)
		;
	def("log", script_wrapper::log_msg);
	def("log_err", script_wrapper::log_error);
	def("log_deb", script_wrapper::log_debug);
	def("log_error", script_wrapper::log_error);
	def("log_debug", script_wrapper::log_debug);
	def("sleep", script_wrapper::sleep);
	//	def("get_module_alias", script_wrapper::get_module_alias);
	//	def("get_script_alias", script_wrapper::get_script_alias);
}

python_script::python_script(unsigned int plugin_id, const std::string base_path, const std::string alias, const script_container& script)
	: alias(alias)
	, base_path(base_path)
	, plugin_id(plugin_id) {
	NSC_DEBUG_MSG_STD("Loading python script: " + script.script.string());
	std::string err;
	if (!script.validate(err)) {
		NSC_LOG_ERROR(err);
		return;
	}
	_exec(script.script.string());
	callFunction("init", plugin_id, alias, utf8::cvt<std::string>(script.alias));
}
python_script::~python_script() {
	callFunction("shutdown");
}
bool python_script::callFunction(const std::string& functionName) {
	try {
		script_wrapper::thread_locker locker;
		try {
			if (!localDict.has_key(functionName))
				return true;
			object scriptFunction = extract<object>(localDict[functionName]);
			if (scriptFunction)
				scriptFunction();
			return true;
		} catch (error_already_set e) {
			script_wrapper::log_exception();
			return false;
		}
	} catch (...) {
		NSC_LOG_ERROR("Unknown exception");
		return false;
	}
}
bool python_script::callFunction(const std::string& functionName, const std::list<std::string> &args) {
	try {
		script_wrapper::thread_locker locker;
		try {
			if (!localDict.has_key(functionName))
				return true;
			object scriptFunction = extract<object>(localDict[functionName]);
			if (scriptFunction)
				scriptFunction(script_wrapper::convert(args));
			return true;
		} catch (error_already_set e) {
			script_wrapper::log_exception();
			return false;
		}
	} catch (...) {
		NSC_LOG_ERROR("Unknown exception");
		return false;
	}
}
bool python_script::callFunction(const std::string& functionName, unsigned int i1, const std::string &s1, const std::string &s2) {
	try {
		script_wrapper::thread_locker locker;
		try {
			if (!localDict.has_key(functionName))
				return true;
			object scriptFunction = extract<object>(localDict[functionName]);
			if (scriptFunction)
				scriptFunction(i1, s1, s2);
			return true;
		} catch (error_already_set e) {
			script_wrapper::log_exception();
			return false;
		}
	} catch (...) {
		NSC_LOG_ERROR("Unknown exception");
		return false;
	}
}
void python_script::_exec(const std::string &scriptfile) {
	try {
		script_wrapper::thread_locker locker;
		try {
			object main_module = import("__main__");
			dict globalDict = extract<dict>(main_module.attr("__dict__"));
			localDict = globalDict.copy();
			localDict.setdefault("__file__", scriptfile);
			//localDict.attr("plugin_id") = plugin_id;
			//localDict.attr("plugin_alias") = alias;

			PyRun_SimpleString("import cStringIO");
			PyRun_SimpleString("import sys");
			PyRun_SimpleString("sys.stderr = cStringIO.StringIO()");
			boost::filesystem::path path = base_path;
			path /= "scripts";
			path /= "python";
			path /= "lib";
			NSC_DEBUG_MSG("Lib path: " + path.string());
			try {
#ifdef WIN32
				//TODO: FIXME: Fix this somehow
				PyRun_SimpleString(("sys.path.append('" + path.generic_string() + "')").c_str());
#else
				PyRun_SimpleString(("sys.path.append('" + path.string() + "')").c_str());
#endif
			} catch (error_already_set e) {
				NSC_LOG_ERROR("Failed to setup env for script: " + scriptfile);
				script_wrapper::log_exception();
				return;
			}

			object ignored = exec_file(scriptfile.c_str(), localDict, localDict);
		} catch (error_already_set e) {
			NSC_LOG_ERROR("Failed to load script: " + scriptfile);
			script_wrapper::log_exception();
		} catch (const std::exception &e) {
			NSC_LOG_ERROR("Failed to load script: " + scriptfile);
			NSC_LOG_ERROR_EXR("python script", e);
		} catch (...) {
			NSC_LOG_ERROR("Failed to load script: " + scriptfile);
			NSC_LOG_ERROR_EX("python script");
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("python");
	}
}
static bool has_init = false;
bool PythonScript::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	alias_ = alias;

	if (mode == NSCAPI::reloadStart) {
		nscapi::core_helper ch(get_core(), get_id());
		BOOST_FOREACH(const std::string &s, script_wrapper::functions::get()->get_commands()) {
			ch.unregister_command(s);
		}
		instances_.clear();
		scripts_.clear();
	}

	try {
		root_ = get_base_path();

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "python");

		settings.alias().add_path_to_settings()
			("LUA SCRIPT SECTION", "Section for the PythonScripts module.")

			("scripts", sh::fun_values_path(boost::bind(&PythonScript::loadScript, this, _1, _2)),
				"PYTHON SCRIPTS SECTION", "A list of scripts available to run from the PythonScript module.",
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
		settings.notify();

		NSC_DEBUG_MSG("boot python");

		bool do_init = false;
		if (!has_init) {
			has_init = true;
			Py_Initialize();
			PyEval_InitThreads();
			//PyEval_ReleaseLock();

			PyEval_SaveThread();
			do_init = true;
		}

		try {
			{
				script_wrapper::thread_locker locker;
				try {
					NSC_DEBUG_MSG("Prepare python");

					PyRun_SimpleString("import cStringIO");
					PyRun_SimpleString("import sys");
					PyRun_SimpleString("sys.stderr = cStringIO.StringIO()");

					if (do_init) {
						NSC_DEBUG_MSG("init python");
						initNSCP();
					}
				} catch (error_already_set e) {
					script_wrapper::log_exception();
				}
			}
			//PyEval_ReleaseLock();
			BOOST_FOREACH(script_container &script, scripts_) {
				instances_.push_back(boost::shared_ptr<python_script>(new python_script(get_id(), root_.string(), alias, script)));
			}
		} catch (std::exception &e) {
			NSC_LOG_ERROR_EXR("load python scripts", e);
		} catch (...) {
			NSC_LOG_ERROR_EX("load python scripts");
		}
	} catch (...) {
		NSC_LOG_ERROR_STD("Exception caught: <UNKNOWN EXCEPTION>");
		return false;
	}
	return true;
}

boost::optional<boost::filesystem::path> PythonScript::find_file(std::string file) {
	std::list<boost::filesystem::path> checks;
	checks.push_back(file);
	checks.push_back(file + ".py");
	checks.push_back(root_ / "scripts" / "python" / file);
	checks.push_back(root_ / "scripts" / "python" / (file + ".py"));
	checks.push_back(root_ / "scripts" / file);
	checks.push_back(root_ / "scripts" / (file + ".py"));
	checks.push_back(root_ / file);
	BOOST_FOREACH(boost::filesystem::path c, checks) {
		NSC_DEBUG_MSG_STD("Looking for: " + c.string());
		if (boost::filesystem::exists(c) && boost::filesystem::is_regular(c))
			return boost::optional<boost::filesystem::path>(c);
	}
	NSC_LOG_ERROR("Script not found: " + file);
	return boost::optional<boost::filesystem::path>();
}

bool PythonScript::loadScript(std::string alias, std::string file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = "";
		}
		boost::optional<boost::filesystem::path> ofile = find_file(file);
		if (!ofile)
			return false;
		script_container::push(scripts_, alias, *ofile);
		NSC_DEBUG_MSG_STD("Adding script: " + alias + " (" + ofile->string() + ")");
		return true;
	} catch (...) {
		NSC_LOG_ERROR_STD("Could not find script: (Unknown exception) " + file);
	}
	return false;
}

bool PythonScript::unloadModule() {
	instances_.clear();
	scripts_.clear();
	//Py_Finalize();
	return true;
}

bool PythonScript::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
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
	std::string command = request.command();
	if (command == "ext-scr" && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module)
		command = "help";
	try {
		if (command == "execute" || command == "exec" || command == "python-script")
			execute_script(request, response);
		else if (command == "list")
			list(request, response);
		else if (command == "help") {
			nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp py [execute|list|install] --help");
		} else
			return false;
		return true;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: ");
	}
	return false;
}


void PythonScript::list(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	bool json = false, lib = false;

	desc.add_options()
		("help", "Show help.")

		("json", po::bool_switch(&json),
			"Return the list in json format.")
		("include-lib", po::bool_switch(&lib),
			"Do not ignore any lib folders.")

		;

	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);
	} catch (const std::exception &e) {
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
		return;
	}
	std::string resp;
	json_spirit::Array data;

	boost::filesystem::path dir = get_core()->expand_path("${scripts}/python");
	boost::filesystem::path rel = get_core()->expand_path("${scripts}/python");
	boost::filesystem::recursive_directory_iterator iter(dir), eod;
	BOOST_FOREACH(boost::filesystem::path const& i, std::make_pair(iter, eod)) {
		std::string s = i.string();
		if (boost::algorithm::starts_with(s, rel.string()))
			s = s.substr(rel.string().size());
		if (s.size() == 0)
			continue;
		if (s[0] == '\\' || s[0] == '/')
			s = s.substr(1);
		boost::filesystem::path clone = i.parent_path();
		if (boost::filesystem::is_regular_file(i) 
			&& !boost::algorithm::contains(clone.string(), "lib") 
			&& boost::ends_with(s, "py")
			&& !boost::ends_with(s, "__init__.py")) {
			if (json) {
				json_spirit::Value v = s;
				data.push_back(v);
			} else {
				resp += s + "\n";
			}

		}
	}
	if (json)
		resp = json_spirit::write(data, json_spirit::raw_utf8);

	nscapi::protobuf::functions::set_response_good(*response, resp);
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

	boost::optional<boost::filesystem::path> ofile = find_file(file);
	if (!ofile) {
		nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file);
		return;
	}
	script_container sc(*ofile);
	python_script script(get_id(), root_.string(), "", sc);
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
			std::string perf = nscapi::protobuf::functions::build_performance_data(line);
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
