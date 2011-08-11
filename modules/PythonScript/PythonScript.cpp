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
#include <strEx.h>
#include <time.h>
#include <error.hpp>
#include <file_helpers.hpp>

#include <boost/python.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/functions.hpp>

#include "script_wrapper.hpp"

PythonScript gPythonScript;

PythonScript::PythonScript() {
}
PythonScript::~PythonScript() {
}

namespace sh = nscapi::settings_helper;

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
		;
	class_<script_wrapper::function_wrapper, boost::shared_ptr<script_wrapper::function_wrapper> >("Functions", no_init)
		.def("get",&script_wrapper::function_wrapper::create)
		.staticmethod("get")
		.def("create",&script_wrapper::function_wrapper::create)
		.staticmethod("create")
		.def("register", &script_wrapper::function_wrapper::register_function)
		.def("register_simple", &script_wrapper::function_wrapper::register_simple_function)
		.def("subscribe", &script_wrapper::function_wrapper::subscribe_function)
		.def("subscribe_simple", &script_wrapper::function_wrapper::subscribe_simple_function)
		;
	class_<script_wrapper::command_wrapper, boost::shared_ptr<script_wrapper::command_wrapper> >("Core", no_init)
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
		;

	enum_<script_wrapper::status>("status")
		.value("CRITICAL", script_wrapper::CRIT)
		.value("WARNING", script_wrapper::WARN)
		.value("UNKNOWN", script_wrapper::UNKNOWN)
		.value("OK", script_wrapper::OK)
		;
	def("log", script_wrapper::log_msg);
}

python_script::python_script(const script_container& script)  {
	_exec(utf8::cvt<std::string>(script.script.string()));
	callFunction("init", utf8::cvt<std::string>(script.alias));
}
python_script::~python_script(){
	callFunction("shutdown");
}
void python_script::callFunction(const std::string& functionName) {
	try	{
		object scriptFunction = extract<object>(localDict[functionName]);
		if( scriptFunction )
			scriptFunction();
	} catch( error_already_set e) {
		script_wrapper::log_exception();
	}
}
void python_script::callFunction(const std::string& functionName, const std::string &str){
	try	{
		object scriptFunction = extract<object>(localDict[functionName]);
		if(scriptFunction)
			scriptFunction(str);
	} catch(error_already_set e) {
		script_wrapper::log_exception();
	}
}
void python_script::_exec(const std::string &scriptfile){
	try	{
		object main_module = import("__main__");
		dict globalDict = extract<dict>(main_module.attr("__dict__"));
		localDict = globalDict.copy();
		object ignored = exec_file(scriptfile.c_str(), localDict, localDict);	
	} catch( error_already_set e) {
		script_wrapper::log_exception();
	}
}

bool PythonScript::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
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

		Py_Initialize();
		try {

			PyRun_SimpleString("import cStringIO");
			PyRun_SimpleString("import sys");
			PyRun_SimpleString("sys.stderr = cStringIO.StringIO()");

			initNSCP();

			BOOST_FOREACH(script_container &script, scripts_) {
				instances_.push_back(boost::shared_ptr<python_script>(new python_script(script)));
			}

		} catch( error_already_set e) {
			script_wrapper::log_exception();
		} catch (std::exception &e) {
			NSC_LOG_ERROR_STD(_T("Exception: Failed to load python scripts: ") + utf8::cvt<std::wstring>(e.what()));
		}
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
	return true;
}

boost::optional<boost::filesystem::wpath> PythonScript::find_file(std::wstring file) {
	std::list<boost::filesystem::wpath> checks;
	checks.push_back(file);
	checks.push_back(root_ / _T("scripts") / _T("python") / file);
	checks.push_back(root_ / _T("scripts") / file);
	checks.push_back(root_ / _T("python") / file);
	checks.push_back(root_ / file);
	BOOST_FOREACH(boost::filesystem::wpath c, checks) {
		NSC_DEBUG_MSG_STD(_T("Looking for: ") + c.string());
		if (boost::filesystem::exists(c))
			return boost::optional<boost::filesystem::wpath>(c);
	}
	NSC_LOG_ERROR(_T("Script not found: ") + file);
	return boost::optional<boost::filesystem::wpath>();
}

bool PythonScript::loadScript(std::wstring alias, std::wstring file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = _T("");
		}
		boost::optional<boost::filesystem::wpath> ofile = find_file(file);
		if (!ofile)
			return false;
		script_container::push(scripts_, alias, *ofile);
		NSC_DEBUG_MSG_STD(_T("Adding script: ") + alias + _T(" (") + ofile->string() + _T(")"));
		return true;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not find script: (Unknown exception) ") + file);
	}
	return false;
}


bool PythonScript::unloadModule() {
	instances_.clear();
	Py_Finalize();
	return true;
}

bool PythonScript::hasCommandHandler() {
	return true;
}
bool PythonScript::hasMessageHandler() {
	return false;
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


NSCAPI::nagiosReturn PythonScript::handleRAWCommand(const wchar_t* command, const std::string &request, std::string &response) {
	boost::shared_ptr<script_wrapper::function_wrapper> inst = script_wrapper::function_wrapper::create();
	std::string cmd = utf8::cvt<std::string>(command);
	if (inst->has_function(cmd)) {
		return inst->exec(cmd, request, response);
	}
	if (inst->has_simple(cmd)) {
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::process_simple_command_request(command, request);
		std::wstring msg, perf;
		NSCAPI::nagiosReturn ret = inst->exec_simple(cmd, data.args, msg, perf);
		return nscapi::functions::process_simple_command_result(data.command, ret, msg, perf, response);
	}
	NSC_LOG_ERROR_STD(_T("Could not find python commands for: ") + command + _T(" (avalible python commands are: ") + inst->get_commands() + _T(")"));
	/*
	if (command == _T("luareload")) {
		return reload(message)?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	*/
	return NSCAPI::returnIgnored;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gPythonScript);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gPythonScript);
