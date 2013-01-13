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

PythonScript::PythonScript() {
}
PythonScript::~PythonScript() {
}

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool PythonScript::loadModule() {
	return false;
}
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

python_script::python_script(unsigned int plugin_id, const std::string alias, const script_container& script) 
	: alias(alias)
	, plugin_id(plugin_id) {
	NSC_DEBUG_MSG_STD(_T("Loading python script: ") + script.script.wstring());
	std::wstring err;
	if (!script.validate(err)) {
		NSC_LOG_ERROR(err);
		return;
	}
	_exec(script.script.filename().string());
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
		NSC_LOG_ERROR(_T("Unknown exception"));
		return false;
	}
}
bool python_script::callFunction(const std::string& functionName, const std::vector<std::wstring> args) {
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
		NSC_LOG_ERROR(_T("Unknown exception"));
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
		NSC_LOG_ERROR(_T("Unknown exception"));
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
			boost::filesystem::path path = GET_CORE()->getBasePath();
			path /= _T("scripts");
			path /= _T("python");
			path /= _T("lib");
			NSC_DEBUG_MSG(_T("Lib path: ") + path.wstring());
			PyRun_SimpleString(("sys.path.append('" + utf8::cvt<std::string>(path.string()) + "')").c_str());

			object ignored = exec_file(scriptfile.c_str(), localDict, localDict);	
		} catch( error_already_set e) {
			script_wrapper::log_exception();
		} catch(...) {
			NSC_LOG_ERROR(_T("Failed to run python script: ") + utf8::to_unicode(scriptfile));
		}
	} catch (...) {
		NSC_LOG_ERROR(_T("Unknown exception"));
	}
}
static bool has_init = false;
bool PythonScript::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	alias_ = alias;
	NSC_DEBUG_MSG_STD(_T("LoadEx in PythonScript as ") + alias);
	try {
		root_ = get_core()->getBasePath();

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, _T("python"));

		settings.alias().add_path_to_settings()
			(_T("LUA SCRIPT SECTION"), _T("Section for the PythonScripts module."))

			(_T("scripts"), sh::fun_values_path(boost::bind(&PythonScript::loadScript, this, _1, _2)), 
			_T("LUA SCRIPTS SECTION"), _T("A list of scripts available to run from the PythonScript module."))
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
				instances_.push_back(boost::shared_ptr<python_script>(new python_script(get_id(), utf8::cvt<std::string>(alias), script)));
			}

		} catch (std::exception &e) {
			NSC_LOG_ERROR_STD(_T("Exception: Failed to load python scripts: ") + utf8::cvt<std::wstring>(e.what()));
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("Exception: Failed to load python scripts"));
		}
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
	return true;
}

boost::optional<boost::filesystem::path> PythonScript::find_file(std::wstring file) {
	std::list<boost::filesystem::path> checks;
	checks.push_back(file);
	checks.push_back((file + _T(".py")));
	checks.push_back(root_ / _T("scripts") / _T("python") / file);
	checks.push_back(root_ / _T("scripts") / _T("python") / (file + _T(".py")));
	checks.push_back(root_ / _T("scripts") / file);
	checks.push_back(root_ / _T("scripts") / (file + _T(".py")));
	checks.push_back(root_ / file);
	BOOST_FOREACH(boost::filesystem::path c, checks) {
		NSC_DEBUG_MSG_STD(_T("Looking for: ") + c.wstring());
		if (boost::filesystem::exists(c) && boost::filesystem::is_regular(c))
			return boost::optional<boost::filesystem::path>(c);
	}
	NSC_LOG_ERROR(_T("Script not found: ") + file);
	return boost::optional<boost::filesystem::path>();
}

NSCAPI::nagiosReturn PythonScript::execute_and_load_python(std::list<std::wstring> args, std::wstring &message) {
	try {
		po::options_description desc("Options for the following commands: (exec, execute)");
		boost::program_options::variables_map vm;
		std::wstring file;
		desc.add_options()
			("help", "Display help")
			("script", po::wvalue<std::wstring>(&file), "The script to run")
			("file", po::wvalue<std::wstring>(&file), "The script to run")
			;

		std::vector<std::wstring> vargs(args.begin(), args.end());
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).allow_unregistered().run();
		po::store(parsed, vm);
		po::notify(vm);

		std::vector<std::wstring> py_args = po::collect_unrecognized(parsed.options, po::include_positional);
		if (vm.count("script") == 0 && vm.count("help") > 0) {
			std::stringstream ss;
			ss << desc;
			message = utf8::to_unicode(ss.str());
			return NSCAPI::returnUNKNOWN;
		} else if (vm.count("help") > 0) {
			py_args.push_back(_T("--help"));
		}

		boost::optional<boost::filesystem::path> ofile = find_file(file);
		if (!ofile)
			return false;
		script_container sc(*ofile);
		python_script script(get_id(), "", sc);
		if (!script.callFunction("__main__", py_args)) {
			message = _T("Failed to execute script: __main__");
			return NSCAPI::returnUNKNOWN;
		}
		message = _T("Script execute successfully...");
		return NSCAPI::returnOK;
	} catch (const std::exception &e) {
		message = _T("Failed to execute script ") + utf8::to_unicode(e.what());
		NSC_LOG_ERROR_STD(message);
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = _T("Failed to execute script ");
		NSC_LOG_ERROR_STD(message);
		return NSCAPI::returnUNKNOWN;
	}
}

bool PythonScript::loadScript(std::wstring alias, std::wstring file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = _T("");
		}
		boost::optional<boost::filesystem::path> ofile = find_file(file);
		if (!ofile)
			return false;
		script_container::push(scripts_, alias, *ofile);
		NSC_DEBUG_MSG_STD(_T("Adding script: ") + alias + _T(" (") + ofile->wstring() + _T(")"));
		return true;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not find script: (Unknown exception) ") + file);
	}
	return false;
}


bool PythonScript::unloadModule() {
	instances_.clear();
	//Py_Finalize();
	return true;
}

bool PythonScript::hasCommandHandler() {
	return true;
}
bool PythonScript::hasMessageHandler() {
	return true;
}
bool PythonScript::hasNotificationHandler() {
	return true;
}

bool PythonScript::reload(std::wstring &message) {
	/*
	bool error = false;
	commands_.clear();
	for (script_list::const_iterator cit = scripts_.begin(); cit != scripts_.end() ; ++cit) {
		try {
			(*cit)->reload(this);
		} catch (script_wrapper::LUAException e) {
			error = true;
			message += _T("Exception when reloading script: ") + (*cit)->get_script() + _T(": ") + e.getMessage();
			NSC_LOG_ERROR_STD(_T("Exception when reloading script: ") + (*cit)->get_script() + _T(": ") + e.getMessage());
		} catch (...) {
			error = true;
			message += _T("Unhandeled Exception when reloading script: ") + (*cit)->get_script();
			NSC_LOG_ERROR_STD(_T("Unhandeled Exception when reloading script: ") + (*cit)->get_script());
		}
	}
	if (!error)
		message = _T("LUA scripts Reloaded...");
	return !error;
	*/
	return false;
}

NSCAPI::nagiosReturn PythonScript::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response) {
	std::wstring command = char_command;
	if (command == _T("help")) {
		std::list<std::wstring> args;
		args.push_back(_T("--help"));
		std::wstring result;
		int ret = execute_and_load_python(args, result);
		nscapi::functions::create_simple_exec_response<std::wstring>(_T("help"), ret, result, response);
		return ret;
	} else if (command == _T("execute-and-load-python") || command == _T("execute-python") || command == _T("python-script")
		|| command == _T("run") || command == _T("execute") || command == _T("exec") || command == _T("")) {
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_exec_request(char_command, request);
		if (data.command == _T("") && data.args.size() == 0)
			return NSCAPI::returnIgnored;
		std::wstring result;
		int ret = execute_and_load_python(data.args, result);
		nscapi::functions::create_simple_exec_response(data.command, ret, result, response);
		return ret;
	}
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	std::string cmd = utf8::cvt<std::string>(char_command);
	if (inst->has_cmdline(cmd)) {
		return inst->handle_exec(cmd, request, response);
	}
	if (inst->has_simple_cmdline(cmd)) {
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_exec_request(char_command, request);
		std::wstring result;
		NSCAPI::nagiosReturn ret = inst->handle_simple_exec(cmd, data.args, result);
		nscapi::functions::create_simple_exec_response(data.command, ret, result, response);
		return ret;
	}
	return NSCAPI::returnIgnored;
}


NSCAPI::nagiosReturn PythonScript::handleRAWCommand(const wchar_t* command, const std::string &request, std::string &response) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	std::string cmd = utf8::cvt<std::string>(command);
	if (inst->has_function(cmd)) {
		return inst->handle_query(cmd, request, response);
	}
	if (inst->has_simple(cmd)) {
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(command, request);
		std::wstring msg, perf;
		NSCAPI::nagiosReturn ret = inst->handle_simple_query(cmd, data.args, msg, perf);
		nscapi::functions::create_simple_query_response(data.command, ret, msg, perf, response);
		return ret;
	}
	NSC_LOG_ERROR_STD(_T("Could not find python commands for: ") + command + _T(" (avalible python commands are: ") + inst->get_commands() + _T(")"));
	/*
	if (command == _T("luareload")) {
		return reload(message)?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	*/
	return NSCAPI::returnIgnored;
}


NSCAPI::nagiosReturn PythonScript::handleRAWNotification(const std::wstring &channel, std::string &request, std::string &response) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create(get_id());
	std::string chnl = utf8::cvt<std::string>(channel);
	if (inst->has_message_handler(chnl)) {
		NSCAPI::nagiosReturn ret = inst->handle_message(chnl, request, response);
		if (ret != NSCAPI::returnIgnored)
			return ret;
	}
	if (inst->has_simple_message_handler(chnl)) {
		std::wstring src, cmd, msg, perf;
		int code = nscapi::functions::parse_simple_submit_request(request, src, cmd, msg, perf);
		int ret = inst->handle_simple_message(chnl, utf8::cvt<std::string>(src), utf8::cvt<std::string>(cmd), code, msg, perf);
		nscapi::functions::create_simple_submit_response(channel, cmd, ret, _T(""), response);
		return ret;
	}
	return NSCAPI::returnIgnored;
}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(PythonScript, _T("python"))
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
NSC_WRAPPERS_CLI_DEF()
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF()
