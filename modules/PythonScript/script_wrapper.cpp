#include "stdafx.h"

#include <strEx.h>
#include "script_wrapper.hpp"

using namespace boost::python;

boost::shared_ptr<script_wrapper::functions> script_wrapper::functions::instance;


void script_wrapper::log_msg(std::wstring x) {
	NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(x));
}

void script_wrapper::log_exception() {
	PyErr_Print();
	boost::python::object sys(boost::python::handle<>(PyImport_ImportModule("sys")));
	boost::python::object err = sys.attr("stderr");
	std::string err_text = boost::python::extract<std::string>(err.attr("getvalue")());
	NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(err_text));
	PyErr_Clear();
}
void script_wrapper::function_wrapper::register_simple_function(std::string name, PyObject* callable, std::string desc) {
	try {
		core->registerCommand(utf8::cvt<std::wstring>(name), utf8::cvt<std::wstring>(desc));
		functions::get()->simple_functions[name] = callable;
		NSC_LOG_MESSAGE_STD(_T("Added simple command: ") + utf8::cvt<std::wstring>(name) + _T(" to list of: ") + get_commands());
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(name));
	}
}
void script_wrapper::function_wrapper::register_function(std::string name, PyObject* callable, std::string desc) {
	try {
	core->registerCommand(utf8::cvt<std::wstring>(name), utf8::cvt<std::wstring>(desc));
	functions::get()->normal_functions[name] = callable;
	NSC_LOG_MESSAGE_STD(_T("Added simple command: ") + utf8::cvt<std::wstring>(name) + _T(" to list of: ") + get_commands());
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(name));
	}
}
int script_wrapper::function_wrapper::exec(const std::string cmd, const std::string &request, std::string &response) const {
	try {
		functions::function_map_type::iterator it = functions::get()->normal_functions.find(cmd);
		if (it == functions::get()->normal_functions.end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find python function: ") + utf8::cvt<std::wstring>(cmd));
			return NSCAPI::returnIgnored;
		}
		tuple ret = boost::python::call<tuple>(it->second, cmd, request);
		if (ret.ptr() == Py_None) {
			return NSCAPI::returnUNKNOWN;
		}
		int ret_code = NSCAPI::returnUNKNOWN;
		if (len(ret) > 0)
			ret_code = extract<int>(ret[0]);
		if (len(ret) > 1)
			response = extract<std::string>(ret[1]);
		return ret_code;
	} catch( error_already_set e) {
		log_exception();
		return NSCAPI::returnUNKNOWN;
	}
}

int script_wrapper::function_wrapper::exec_simple(const std::string cmd, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_functions.find(cmd);
		if (it == functions::get()->simple_functions.end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find python function: ") + utf8::cvt<std::wstring>(cmd));
			return NSCAPI::returnIgnored;
		}

		boost::python::list l;
		BOOST_FOREACH(std::wstring a, arguments) {
			l.append(utf8::cvt<std::string>(a));
		}
		tuple ret = boost::python::call<tuple>(it->second, l);
		if (ret.ptr() == Py_None) {
			msg = _T("None");
			return NSCAPI::returnUNKNOWN;
		}
		int ret_code = NSCAPI::returnUNKNOWN;
		if (len(ret) > 0)
			ret_code = extract<int>(ret[0]);
		if (len(ret) > 1)
			msg = utf8::cvt<std::wstring>(extract<std::string>(ret[1]));
		if (len(ret) > 2)
			perf = utf8::cvt<std::wstring>(extract<std::string>(ret[2]));
		return ret_code;
	} catch( error_already_set e) {
		log_exception();
		msg = _T("Exception in: ") + utf8::cvt<std::wstring>(cmd);
		return NSCAPI::returnUNKNOWN;
	}
}

bool script_wrapper::function_wrapper::has_function(const std::string command) {
	return functions::get()->normal_functions.find(command) != functions::get()->normal_functions.end();
}
bool script_wrapper::function_wrapper::has_simple(const std::string command) {
	return functions::get()->simple_functions.find(command) != functions::get()->simple_functions.end();
}
std::wstring script_wrapper::function_wrapper::get_commands() {
	std::wstring str;
	BOOST_FOREACH(const functions::function_map_type::value_type& i, functions::get()->normal_functions) {
		std::wstring tmp = utf8::cvt<std::wstring>(i.first);
		strEx::append_list(str, tmp, _T(", "));
	}
	BOOST_FOREACH(const functions::function_map_type::value_type& i, functions::get()->simple_functions) {
		std::wstring tmp = utf8::cvt<std::wstring>(i.first);
		strEx::append_list(str, tmp, _T(", "));
	}
	return str;
}



std::list<std::wstring> script_wrapper::command_wrapper::convert(list lst) {
	std::list<std::wstring> ret;
	for (int i = 0;i<len(lst);i++)
		ret.push_back(utf8::cvt<std::wstring>(extract<std::string>(lst[i])));
	return ret;
}
tuple script_wrapper::command_wrapper::simple_query(std::string command, list args) {
	std::wstring msg, perf;
	int ret = core->InjectSimpleCommand(utf8::cvt<std::wstring>(command), convert(args), msg, perf);
	return make_tuple(ret,utf8::cvt<std::string>(msg), utf8::cvt<std::string>(perf));
}
tuple script_wrapper::command_wrapper::query(std::string command, std::string request) {
	std::string response;
	int ret = core->InjectCommand(utf8::cvt<std::wstring>(command), request, response);
	return make_tuple(ret,response);
}


std::string script_wrapper::settings_wrapper::get_string(std::string path, std::string key, std::string def) {
	return utf8::cvt<std::string>(core->getSettingsString(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), utf8::cvt<std::wstring>(def)));
}
void script_wrapper::settings_wrapper::set_string(std::string path, std::string key, std::string value) {
	core->SetSettingsString(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), utf8::cvt<std::wstring>(value));
}
bool script_wrapper::settings_wrapper::get_bool(std::string path, std::string key, bool def) {
	return core->getSettingsBool(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), def);
}
void script_wrapper::settings_wrapper::set_bool(std::string path, std::string key, bool value) {
	core->SetSettingsInt(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), value);
}
int script_wrapper::settings_wrapper::get_int(std::string path, std::string key, int def) {
	return core->getSettingsInt(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), def);
}
void script_wrapper::settings_wrapper::set_int(std::string path, std::string key, int value) {
	core->SetSettingsInt(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), value);
}
std::list<std::string> script_wrapper::settings_wrapper::get_section(std::string path) {
	std::list<std::string> ret;
	BOOST_FOREACH(std::wstring s, core->getSettingsSection(utf8::cvt<std::wstring>(path))) {
		ret.push_back(utf8::cvt<std::string>(s));
	}
	return ret;
}
void script_wrapper::settings_wrapper::save() {
	core->settings_save();
}

NSCAPI::settings_type script_wrapper::settings_wrapper::get_type(std::string stype) {
	if (stype == "string" || stype == "str" || stype == "s")
		return NSCAPI::key_string;
	if (stype == "integer" || stype == "int" || stype == "i")
		return NSCAPI::key_integer;
	if (stype == "bool" || stype == "b")
		return NSCAPI::key_bool;
	NSC_LOG_ERROR_STD(_T("Invalid settings type"));
	return NSCAPI::key_string;
}
void script_wrapper::settings_wrapper::settings_register_key(std::string path, std::string key, std::string stype, std::string title, std::string description, std::string defaultValue) {
	NSCAPI::settings_type type = get_type(stype);
	core->settings_register_key(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), type, utf8::cvt<std::wstring>(title), utf8::cvt<std::wstring>(description), utf8::cvt<std::wstring>(defaultValue), false);
}
void script_wrapper::settings_wrapper::settings_register_path(std::string path, std::string title, std::string description) {
	core->settings_register_path(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(title), utf8::cvt<std::wstring>(description), false);
}
