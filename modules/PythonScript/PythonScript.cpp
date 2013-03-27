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
#include "PythonScript.h"
#include <time.h>
#include <error.hpp>

#include <boost/python.hpp>
#include <boost/program_options.hpp>

#include <strEx.h>
#include <file_helpers.hpp>
#include <settings/client/settings_client.hpp>
#include <nscapi/functions.hpp>

#include "script_wrapper.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;
using namespace boost::python;

BOOST_PYTHON_MODULE(NSCP)
{
	class_<script_wrapper::settings_wrapper, boost::shared_ptr<script_wrapper::settings_wrapper> >("Settings", no_init)
		.def("get",&script_wrapper::settings_wrapper::create)
		.staticmethod("get")
		.def("create",&script_wrapper::settings_wrapper::create)
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
		.def("get",&script_wrapper::function_wrapper::create)
		.staticmethod("get")
		.def("create",&script_wrapper::function_wrapper::create)
		.staticmethod("create")
		.def("function", &script_wrapper::function_wrapper::register_function)
		.def("simple_function", &script_wrapper::function_wrapper::register_simple_function)
		.def("cmdline", &script_wrapper::function_wrapper::register_cmdline)
		.def("simple_cmdline", &script_wrapper::function_wrapper::register_simple_cmdline)
		.def("subscription", &script_wrapper::function_wrapper::subscribe_function)
		.def("simple_subscription", &script_wrapper::function_wrapper::subscribe_simple_function)
		.def("query", &script_wrapper::function_wrapper::query)
		;
	class_<script_wrapper::command_wrapper, boost::shared_ptr<script_wrapper::command_wrapper> >("Core", init<>())
		.def("get",&script_wrapper::command_wrapper::create)
		.staticmethod("get")
		.def("create",&script_wrapper::command_wrapper::create)
		.staticmethod("create")
		.def("simple_query", &script_wrapper::command_wrapper::simple_query)
		.def("query", &script_wrapper::command_wrapper::query)
		.def("simple_exec", &script_wrapper::command_wrapper::simple_exec)
		.def("exec", &script_wrapper::command_wrapper::exec)
		.def("simple_submit", &script_wrapper::command_wrapper::simple_submit)
		.def("submit", &script_wrapper::command_wrapper::submit)
		.def("reload", &script_wrapper::command_wrapper::reload)
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
python_script::~python_script(){
	callFunction("shutdown");
}
bool python_script::callFunction(const std::string& functionName) {
	try {
		script_wrapper::thread_locker locker;
		try	{
			if (!localDict.has_key(functionName))
				return true;
			object scriptFunction = extract<object>(localDict[functionName]);
			if( scriptFunction )
				scriptFunction();
			return true;
		} catch( error_already_set e) {
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
		try	{
			if (!localDict.has_key(functionName))
				return true;
			object scriptFunction = extract<object>(localDict[functionName]);
			if (scriptFunction)
				scriptFunction(script_wrapper::convert(args));
			return true;
		} catch( error_already_set e) {
			script_wrapper::log_exception();
			return false;
		}
	} catch (...) {
		NSC_LOG_ERROR("Unknown exception");
		return false;
	}
}
bool python_script::callFunction(const std::string& functionName, unsigned int i1, const std::string &s1, const std::string &s2){
	try {
		script_wrapper::thread_locker locker;
		try	{
			if (!localDict.has_key(functionName))
				return true;
			object scriptFunction = extract<object>(localDict[functionName]);
			if(scriptFunction)
				scriptFunction(i1, s1, s2);
			return true;
		} catch(error_already_set e) {
			script_wrapper::log_exception();
			return false;
		}
	} catch (...) {
		NSC_LOG_ERROR("Unknown exception");
		return false;
	}
}
void python_script::_exec(const std::string &scriptfile){
	try {
		script_wrapper::thread_locker locker;
		try	{
			object main_module = import("__main__");
			dict globalDict = extract<dict>(main_module.attr("__dict__"));
			localDict = globalDict.copy();
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
#ifdef WIN32
			//TODO: FIXME: Fix this somehow
			PyRun_SimpleString(("sys.path.append('" + path.generic_string() + "')").c_str());
#else
			PyRun_SimpleString(("sys.path.append('" + path.string() + "')").c_str());
#endif

			object ignored = exec_file(scriptfile.c_str(), localDict, localDict);	
		} catch( error_already_set e) {
			script_wrapper::log_exception();
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("python script", e);
		} catch(...) {
			NSC_LOG_ERROR_EX("python script");
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("python");
	}
}
static bool has_init = false;
bool PythonScript::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	alias_ = alias;
	try {
		root_ = get_base_path();

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "python");

		settings.alias().add_path_to_settings()
			("LUA SCRIPT SECTION", "Section for the PythonScripts module.")

			("scripts", sh::fun_values_path(boost::bind(&PythonScript::loadScript, this, _1, _2)), 
			"LUA SCRIPTS SECTION", "A list of scripts available to run from the PythonScript module.")
			;

		settings.register_all();
		settings.notify();

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

					PyRun_SimpleString("import cStringIO");
					PyRun_SimpleString("import sys");
					PyRun_SimpleString("sys.stderr = cStringIO.StringIO()");

					if (do_init)
						initNSCP();

				} catch( error_already_set e) {
					script_wrapper::log_exception();
				}

			}
			//PyEval_ReleaseLock();
			BOOST_FOREACH(script_container &script, scripts_) {
				instances_.push_back(boost::shared_ptr<python_script>(new python_script(get_id(), root_.string(), utf8::cvt<std::string>(alias), script)));
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
	//Py_Finalize();
	return true;
}

bool PythonScript::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_cmdline(request.command())) {
		std::string buffer;
		int ret = inst->handle_exec(request.command(), request_message.SerializeAsString(), buffer);
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
		for (int i=0;i<request.arguments_size();i++)
			args.push_back(request.arguments(i));
		std::string result;
		NSCAPI::nagiosReturn ret = inst->handle_simple_exec(request.command(), args, result);
		response->set_message(result);
		response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
	}
	if (request.command() != "execute-and-load-python" && request.command() != "execute-python" && request.command() != "python-script"
		&& request.command() != "run" && request.command() != "execute" && request.command() != "exec" && request.command() != "") {
			return false;
	}

	try {
		po::options_description desc = nscapi::program_options::create_desc(request);
		std::string file;
		desc.add_options()
			("script", po::value<std::string>(&file), "The script to run")
			("file", po::value<std::string>(&file), "The script to run")
			;
		boost::program_options::variables_map vm;
		nscapi::program_options::unrecognized_map script_options;
		if (!nscapi::program_options::process_arguments_unrecognized(vm, script_options, desc, request, *response))
			return true;

		boost::optional<boost::filesystem::path> ofile = find_file(file);
		if (!ofile) {
			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file);
			return true;
		}
		script_container sc(*ofile);
		python_script script(get_id(), root_.string(), "", sc);
		std::list<std::string> ops(script_options.begin(), script_options.end());
		if (!script.callFunction("__main__", ops)) {
			nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute __main__");
			return true;
		}
		nscapi::protobuf::functions::set_response_good(*response, "TODO: Collect info messages here!");
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute script " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute script.");
	}
	return true;
}


void PythonScript::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_function(request.command())) {
		std::string buffer;
		if (inst->handle_query(request.command(), request_message.SerializeAsString(), buffer) != NSCAPI::isSuccess) {
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
		for (int i=0;i<request.arguments_size();i++)
			args.push_back(request.arguments(i));
		std::string msg, perf;
		NSCAPI::nagiosReturn ret = inst->handle_simple_query(request.command(), args, msg, perf);
		nscapi::protobuf::functions::parse_performance_data(response, perf);
		response->set_message(msg);
		response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
	}
}


void PythonScript::handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	if (inst->has_message_handler(channel)) {
		std::string buffer;
		if (inst->handle_message(channel, request_message.SerializeAsString(), buffer) == NSCAPI::isSuccess) {
			Plugin::SubmitResponseMessage local_response;
			local_response.ParseFromString(buffer);
			if (local_response.payload_size() == 1) {
				response->CopyFrom(local_response.payload(0));
				return;
			}
		}
	}
	if (inst->has_simple_message_handler(channel)) {
		std::string perf = nscapi::protobuf::functions::build_performance_data(request);
		if (inst->handle_simple_message(channel, request.source(), request.command(), request.result(), request.message(), perf) != NSCAPI::isSuccess)
			return nscapi::protobuf::functions::set_response_bad(*response, "Invalid response: " + channel);
		return nscapi::protobuf::functions::set_response_good(*response, "");
	}
	return nscapi::protobuf::functions::set_response_bad(*response, "Unable to process message: " + channel);
}
