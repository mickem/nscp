#include "stdafx.h"

#include <strEx.h>
#include "script_wrapper.hpp"
#include "PythonScript.h"
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <boost/thread.hpp>

using namespace boost::python;
namespace py = boost::python;

boost::shared_ptr<script_wrapper::functions> script_wrapper::functions::instance;

//extern PythonScript gPythonScript;


script_wrapper::status script_wrapper::nagios_return_to_py(int code) {
	if (code == NSCAPI::returnOK)
		return OK;
	if (code == NSCAPI::returnWARN)
		return WARN;
	if (code == NSCAPI::returnCRIT)
		return CRIT;
	if (code == NSCAPI::returnUNKNOWN)
		return UNKNOWN;
	NSC_LOG_ERROR_STD(_T("Invalid return code: ") + strEx::itos(code));
	return UNKNOWN;
}
int script_wrapper::py_to_nagios_return(status code) {
	NSCAPI::nagiosReturn c = NSCAPI::returnUNKNOWN;
	if (code == OK)
		return NSCAPI::returnOK;
	if (code == WARN)
		return NSCAPI::returnWARN;
	if (code == CRIT)
		return NSCAPI::returnCRIT;
	if (code == UNKNOWN)
		return NSCAPI::returnUNKNOWN;
	NSC_LOG_ERROR_STD(_T("Invalid return code: ") + strEx::itos(c));
	return NSCAPI::returnUNKNOWN;
}


std::string pystr(object o) {
	try {
		if (o.ptr() == Py_None)
			return "";
		if(PyUnicode_Check(o.ptr())) {
			std::string s = PyBytes_AsString(PyUnicode_AsEncodedString(o.ptr(), "utf-8", "Error"));
			return s;
		}
		return extract<std::string>(o);
	} catch (...) {
		NSC_LOG_ERROR(_T("Failed to convert python string"));
		return "Unable to convert python string";
	}
}
std::string pystr(boost::python::api::object_item o) {
	try {
		object po = o;
		return pystr(po);
	} catch (...) {
		NSC_LOG_ERROR(_T("Failed to convert python string"));
		return "Unable to convert python string";
	}
}

object pystr(std::wstring str) {
	return boost::python::object(boost::python::handle<>(PyUnicode_FromString(utf8::cvt<std::string>(str).c_str())));
}

std::wstring pywstr(object o) {
	return utf8::cvt<std::wstring>(pystr(o));
}
std::wstring pywstr(boost::python::api::object_item o) {
	return utf8::cvt<std::wstring>(pystr(o));
}


std::list<std::wstring> script_wrapper::convert(py::list lst) {
	std::list<std::wstring> ret;
	for (int i = 0;i<len(lst);i++) {
		try {
			extract<std::string> es(lst[i]);
			extract<long long> ei(lst[i]);
			if (es.check())
				ret.push_back(utf8::cvt<std::wstring>(es()));
			else if (ei.check())
				ret.push_back(strEx::itos(ei()));
			else
				NSC_LOG_ERROR_STD(_T("Failed to convert object in list"));
		} catch( error_already_set e) {
			log_exception();
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("Failed to parse list"));
		}
	}
	return ret;
}
py::list script_wrapper::convert(std::list<std::wstring> lst) {
	py::list ret;
	BOOST_FOREACH(std::wstring s, lst) {
		ret.append(utf8::cvt<std::string>(s));
	}
	return ret;
}


void script_wrapper::log_msg(object x) {
	std::wstring msg = pywstr(x);
	{
		thread_unlocker unlocker;
		NSC_LOG_MESSAGE(msg);
	}
}
void script_wrapper::log_error(object x) {
	std::wstring msg = pywstr(x);
	{
		thread_unlocker unlocker;
		NSC_LOG_ERROR_STD(msg);
	}
}
void script_wrapper::log_debug(object x) {
	std::wstring msg = pywstr(x);
	{
		thread_unlocker unlocker;
		NSC_DEBUG_MSG(msg);
	}
}
void script_wrapper::sleep(unsigned int ms) {
	{
		thread_unlocker unlocker;
		{
			boost::this_thread::sleep(boost::posix_time::milliseconds(ms));

		}
	}
}
/*
std::string script_wrapper::get_alias() {
	return utf8::cvt<std::string>(gPythonScript.get_alias());
}
*/

void script_wrapper::log_exception() {
	try {
		PyErr_Print();
		boost::python::object sys(boost::python::handle<>(PyImport_ImportModule("sys")));
		boost::python::object err = sys.attr("stderr");
		std::string err_text = boost::python::extract<std::string>(err.attr("getvalue")());
		NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(err_text));
		PyErr_Clear();
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to parse error: ") + utf8::cvt<std::wstring>(e.what()));
		PyErr_Clear();
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to parse python error"));
		PyErr_Clear();
	}
}

void script_wrapper::function_wrapper::subscribe_simple_function(std::string channel, PyObject* callable) {
	try {
		core->registerSubmissionListener(plugin_id, utf8::cvt<std::wstring>(channel));
		boost::python::handle<> h(boost::python::borrowed(callable));
		//return boost::python::object o(h);
		functions::get()->simple_handler[channel] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to subscribe to channel ") + utf8::cvt<std::wstring>(channel) + _T(": ") + utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to subscribe to channel ") + utf8::cvt<std::wstring>(channel));
	}
}
void script_wrapper::function_wrapper::subscribe_function(std::string channel, PyObject* callable) {
	try {
		core->registerSubmissionListener(plugin_id, utf8::cvt<std::wstring>(channel));
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->normal_handler[channel] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to subscribe to channel ") + utf8::cvt<std::wstring>(channel) + _T(": ") + utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to subscribe to channel ") + utf8::cvt<std::wstring>(channel));
	}
}


void script_wrapper::function_wrapper::register_simple_function(std::string name, PyObject* callable, std::string desc) {
	try {
		core->registerCommand(plugin_id, utf8::cvt<std::wstring>(name), utf8::cvt<std::wstring>(desc));
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->simple_functions[name] = h;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register functions: ") + utf8::cvt<std::wstring>(name));
	}
}
void script_wrapper::function_wrapper::register_function(std::string name, PyObject* callable, std::string desc) {
	try {
	core->registerCommand(plugin_id, utf8::cvt<std::wstring>(name), utf8::cvt<std::wstring>(desc));
	boost::python::handle<> h(boost::python::borrowed(callable));
	functions::get()->normal_functions[name] = h;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register functions: ") + utf8::cvt<std::wstring>(name));
	}
}
void script_wrapper::function_wrapper::register_simple_cmdline(std::string name, PyObject* callable) {
	try {
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->simple_cmdline[name] = h;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(name));
	}
}
void script_wrapper::function_wrapper::register_cmdline(std::string name, PyObject* callable) {
	try {
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->normal_cmdline[name] = h;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(name));
	}
}
int script_wrapper::function_wrapper::handle_query(const std::string cmd, const std::string &request, std::string &response) const {
	try {
		functions::function_map_type::iterator it = functions::get()->normal_functions.find(cmd);
		if (it == functions::get()->normal_functions.end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find python function: ") + utf8::cvt<std::wstring>(cmd));
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				tuple ret = boost::python::call<tuple>(boost::python::object(it->second).ptr(), cmd, request);
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
	} catch(...) {
		NSC_LOG_ERROR_STD(_T("Exception in ") + utf8::cvt<std::wstring>(cmd));
		return NSCAPI::returnUNKNOWN;
	}
}

int script_wrapper::function_wrapper::handle_simple_query(const std::string cmd, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_functions.find(cmd);
		if (it == functions::get()->simple_functions.end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find python function: ") + utf8::cvt<std::wstring>(cmd));
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;

			try {
				py::list l;
				BOOST_FOREACH(std::wstring a, arguments) {
					l.append(utf8::cvt<std::string>(a));
				}
				object ret = boost::python::call<object>(boost::python::object(it->second).ptr(), l);
				if (ret.ptr() == Py_None) {
					msg = _T("None");
					return NSCAPI::returnUNKNOWN;
				}
				int ret_code = NSCAPI::returnUNKNOWN;
				if (len(ret) > 0)
					ret_code = extract<int>(ret[0]);
				if (len(ret) > 1)
					msg = pywstr(ret[1]);
				if (len(ret) > 2)
					perf = pywstr(ret[2]);
				return ret_code;
			} catch( error_already_set e) {
				log_exception();
				msg = _T("Exception in: ") + utf8::cvt<std::wstring>(cmd);
				return NSCAPI::returnUNKNOWN;
			}
		}
	} catch(const std::exception &e) {
		msg = _T("Exception in ") + utf8::cvt<std::wstring>(cmd) + _T(": ") + utf8::cvt<std::wstring>(e.what());
		return NSCAPI::returnUNKNOWN;
	} catch(...) {
		msg = _T("Exception in ") + utf8::cvt<std::wstring>(cmd);
		return NSCAPI::returnUNKNOWN;
	}
}

bool script_wrapper::function_wrapper::has_function(const std::string command) {
	return functions::get()->normal_functions.find(command) != functions::get()->normal_functions.end();
}
bool script_wrapper::function_wrapper::has_simple(const std::string command) {
	return functions::get()->simple_functions.find(command) != functions::get()->simple_functions.end();
}

int script_wrapper::function_wrapper::handle_exec(const std::string cmd, const std::string &request, std::string &response) const {
	try {
		functions::function_map_type::iterator it = functions::get()->normal_cmdline.find(cmd);
		if (it == functions::get()->normal_cmdline.end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find python function: ") + utf8::cvt<std::wstring>(cmd));
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				tuple ret = boost::python::call<tuple>(boost::python::object(it->second).ptr(), cmd, request);
				if (ret.ptr() == Py_None) {
					return NSCAPI::returnUNKNOWN;
				}
				int ret_code = NSCAPI::returnUNKNOWN;
				if (len(ret) > 0)
					ret_code = extract<int>(ret[0]);
				if (len(ret) > 1)
					response = pystr(ret[1]);
				return ret_code;
			} catch( error_already_set e) {
				log_exception();
				return NSCAPI::returnUNKNOWN;
			}
		}
	} catch(const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception in ") + utf8::cvt<std::wstring>(cmd) + _T(": ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::returnUNKNOWN;
	} catch(...) {
		NSC_LOG_ERROR_STD(_T("Exception in ") + utf8::cvt<std::wstring>(cmd));
		return NSCAPI::returnUNKNOWN;
	}
}

int script_wrapper::function_wrapper::handle_simple_exec(const std::string cmd, std::list<std::wstring> arguments, std::wstring &result) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_cmdline.find(cmd);
		if (it == functions::get()->simple_cmdline.end()) {
			result = _T("Failed to find python function: ") + utf8::cvt<std::wstring>(cmd);
			NSC_LOG_ERROR_STD(result);
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				tuple ret = boost::python::call<tuple>(boost::python::object(it->second).ptr(), convert(arguments));
				if (ret.ptr() == Py_None) {
					result = _T("None");
					return NSCAPI::returnUNKNOWN;
				}
				int ret_code = NSCAPI::returnUNKNOWN;
				if (len(ret) > 0)
					ret_code = extract<int>(ret[0]);
				if (len(ret) > 1)
					result = utf8::cvt<std::wstring>(extract<std::string>(ret[1]));
				return ret_code;
			} catch( error_already_set e) {
				log_exception();
				result = _T("Exception in: ") + utf8::cvt<std::wstring>(cmd);
				return NSCAPI::returnUNKNOWN;
			}
		}
	} catch(const std::exception &e) {
		result = _T("Exception in ") + utf8::cvt<std::wstring>(cmd) + _T(": ") + utf8::cvt<std::wstring>(e.what());
		return NSCAPI::returnUNKNOWN;
	} catch(...) {
		result = _T("Exception in ") + utf8::cvt<std::wstring>(cmd);
		return NSCAPI::returnUNKNOWN;
	}
}


bool script_wrapper::function_wrapper::has_message_handler(const std::string channel) {
	return functions::get()->normal_handler.find(channel) != functions::get()->normal_handler.end();
}
bool script_wrapper::function_wrapper::has_simple_message_handler(const std::string channel) {
	return functions::get()->simple_handler.find(channel) != functions::get()->simple_handler.end();
}

int script_wrapper::function_wrapper::handle_message(const std::string channel, const std::string &request, std::string &response) const {
	try {
		functions::function_map_type::iterator it = functions::get()->normal_handler.find(channel);
		if (it == functions::get()->normal_handler.end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find python handler: ") + utf8::cvt<std::wstring>(channel));
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			int ret_code = NSCAPI::returnIgnored;
			try {
				object ret = boost::python::call<object>(boost::python::object(it->second).ptr(), channel, request);
				if (ret.ptr() == Py_None) {
					return NSCAPI::returnIgnored;
				}
				if (len(ret) > 0)
					ret_code = extract<bool>(ret[0])?NSCAPI::isSuccess:NSCAPI::returnIgnored;
				if (len(ret) > 1)
					response = extract<std::string>(ret[1]);
			} catch( error_already_set e) {
				log_exception();
				return NSCAPI::returnUNKNOWN;
			}
			return ret_code;
		}
	} catch(const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception in ") + utf8::cvt<std::wstring>(channel) + _T(": ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::returnUNKNOWN;
	} catch(...) {
		NSC_LOG_ERROR_STD(_T("Exception in ") + utf8::cvt<std::wstring>(channel));
		return NSCAPI::returnUNKNOWN;
	}
}
int script_wrapper::function_wrapper::handle_simple_message(const std::string channel, const std::string source, const std::string command, int code, std::wstring &msg, std::wstring &perf) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_handler.find(channel);
		if (it == functions::get()->simple_handler.end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find python handler: ") + utf8::cvt<std::wstring>(channel));
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				object ret = boost::python::call<object>(boost::python::object(it->second).ptr(), channel, source, command, nagios_return_to_py(code), pystr(msg), utf8::cvt<std::string>(perf));
				int ret_code = NSCAPI::returnIgnored;
				if (ret.ptr() == Py_None) {
					ret_code = NSCAPI::isSuccess;
				} else {
					ret_code = extract<bool>(ret)?NSCAPI::isSuccess:NSCAPI::returnIgnored;
				}
				return ret_code;
			} catch( error_already_set e) {
				log_exception();
				return NSCAPI::hasFailed;
			}
		}
	} catch(const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception in ") + utf8::cvt<std::wstring>(channel) + _T(": ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::hasFailed;
	} catch(...) {
		NSC_LOG_ERROR_STD(_T("Exception in ") + utf8::cvt<std::wstring>(channel));
		return NSCAPI::hasFailed;
	}
}






bool script_wrapper::function_wrapper::has_cmdline(const std::string command) {
	return functions::get()->normal_cmdline.find(command) != functions::get()->normal_cmdline.end();
}
bool script_wrapper::function_wrapper::has_simple_cmdline(const std::string command) {
	return functions::get()->simple_cmdline.find(command) != functions::get()->simple_cmdline.end();
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

//////////////////////////////////////////////////////////////////////////
// Callouts from python into NSClient++
//
tuple script_wrapper::command_wrapper::simple_submit(std::string channel, std::string command, status code, std::string message, std::string perf) {
	NSCAPI::nagiosReturn c = py_to_nagios_return(code);
	std::wstring wmessage = utf8::cvt<std::wstring>(message);
	std::wstring wperf = utf8::cvt<std::wstring>(perf);
	std::wstring wchannel = utf8::cvt<std::wstring>(channel);
	std::wstring wcommand = utf8::cvt<std::wstring>(command);
	std::wstring wresp;
	bool ret = false;
	{
		thread_unlocker unlocker;
		ret = nscapi::core_helper::submit_simple_message(wchannel, wcommand, c, wmessage, wperf, wresp);
	}
	return make_tuple(ret,utf8::cvt<std::string>(wresp));
}
tuple script_wrapper::command_wrapper::submit(std::string channel, std::string request) {
	std::wstring wchannel = utf8::cvt<std::wstring>(channel);
	std::string response;
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = core->submit_message(wchannel, request, response);
	}
	std::wstring err;
	nscapi::functions::parse_simple_submit_response(response, err);
	return make_tuple(ret==NSCAPI::isSuccess,err);
}

bool script_wrapper::command_wrapper::reload(std::string module) {
	std::wstring wmodule = utf8::cvt<std::wstring>(module);
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = core->reload(wmodule);
	}
	return ret==NSCAPI::isSuccess;
}

std::string script_wrapper::command_wrapper::expand_path(std::string aPath) {
	thread_unlocker unlocker;
	return utf8::cvt<std::string>(core->expand_path(utf8::cvt<std::wstring>(aPath)));
}



tuple script_wrapper::command_wrapper::simple_query(std::string command, py::list args) {
	std::wstring msg, perf;
	const std::list<std::wstring> ws_argument = convert(args);
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = nscapi::core_helper::simple_query(utf8::cvt<std::wstring>(command), ws_argument, msg, perf);
	}
	return make_tuple(nagios_return_to_py(ret),utf8::cvt<std::string>(msg), utf8::cvt<std::string>(perf));
}
tuple script_wrapper::command_wrapper::query(std::string command, std::string request) {
	std::string response;
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = core->query(utf8::cvt<std::wstring>(command), request, response);
	}
	return make_tuple(ret,response);
}

tuple script_wrapper::command_wrapper::simple_exec(std::string target, std::string command, py::list args) {
	try {
		std::list<std::wstring> result;
		int ret = 0;
		const std::wstring ws_target = utf8::cvt<std::wstring>(target);
		const std::wstring ws_command = utf8::cvt<std::wstring>(command);
		const std::list<std::wstring> ws_argument = convert(args);
		{
			thread_unlocker unlocker;
			ret = nscapi::core_helper::exec_simple_command(ws_target, ws_command, ws_argument, result);
		}
		return make_tuple(ret, convert(result));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to execute ") + utf8::cvt<std::wstring>(command) + _T(": ") + utf8::cvt<std::wstring>(e.what()));
		return make_tuple(false,utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to execute ") + utf8::cvt<std::wstring>(command));
		return make_tuple(false,utf8::cvt<std::wstring>(command));
	}
}
tuple script_wrapper::command_wrapper::exec(std::string target, std::string command, std::string request) {
	try {
		std::string response;
		int ret = 0;
		{
			thread_unlocker unlocker;
			ret = core->exec_command(utf8::cvt<std::wstring>(target), utf8::cvt<std::wstring>(command), request, response);
		}
		return make_tuple(ret, response);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to execute ") + utf8::cvt<std::wstring>(command) + _T(": ") + utf8::cvt<std::wstring>(e.what()));
		return make_tuple(false,utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to execute ") + utf8::cvt<std::wstring>(command));
		return make_tuple(false,utf8::cvt<std::wstring>(command));
	}
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
	core->settings_register_key(plugin_id, utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), type, utf8::cvt<std::wstring>(title), utf8::cvt<std::wstring>(description), utf8::cvt<std::wstring>(defaultValue), false);
}
void script_wrapper::settings_wrapper::settings_register_path(std::string path, std::string title, std::string description) {
	core->settings_register_path(plugin_id, utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(title), utf8::cvt<std::wstring>(description), false);
}
