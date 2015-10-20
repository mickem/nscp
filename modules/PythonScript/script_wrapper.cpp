#include <strEx.h>
#include "script_wrapper.hpp"
#include "PythonScript.h"
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <boost/thread.hpp>

using namespace boost::python;
namespace py = boost::python;

boost::shared_ptr<script_wrapper::functions> script_wrapper::functions::instance;

nscapi::core_wrapper* get_core() {
	return  nscapi::plugin_singleton->get_core();
}

script_wrapper::status script_wrapper::nagios_return_to_py(int code) {
	if (code == NSCAPI::returnOK)
		return OK;
	if (code == NSCAPI::returnWARN)
		return WARN;
	if (code == NSCAPI::returnCRIT)
		return CRIT;
	if (code == NSCAPI::returnUNKNOWN)
		return UNKNOWN;
	NSC_LOG_ERROR_STD("Invalid return code: " + strEx::s::xtos(code));
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
	NSC_LOG_ERROR_STD("Invalid return code: "+ strEx::s::xtos(c));
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
		NSC_LOG_ERROR("Failed to convert python string");
		return "Unable to convert python string";
	}
}
std::string pystr(boost::python::api::object_item o) {
	try {
		object po = o;
		return pystr(po);
	} catch (...) {
		NSC_LOG_ERROR("Failed to convert python string");
		return "Unable to convert python string";
	}
}

object pystr(std::wstring str) {
	return boost::python::object(boost::python::handle<>(PyUnicode_FromString(utf8::cvt<std::string>(str).c_str())));
}
object pystr(std::string str) {
	return boost::python::object(boost::python::handle<>(PyUnicode_FromString(str.c_str())));
}

std::wstring pywstr(object o) {
	return utf8::cvt<std::wstring>(pystr(o));
}
std::wstring pywstr(boost::python::api::object_item o) {
	return utf8::cvt<std::wstring>(pystr(o));
}


std::list<std::string> script_wrapper::convert(py::list lst) {
	std::list<std::string> ret;
	for (int i = 0;i<len(lst);i++) {
		try {
			extract<std::string> es(lst[i]);
			extract<long long> ei(lst[i]);
			if (es.check())
				ret.push_back(es());
			else if (ei.check())
				ret.push_back(strEx::s::xtos(ei()));
			else
				NSC_LOG_ERROR_STD("Failed to convert object in list");
		} catch( error_already_set e) {
			log_exception();
		} catch (...) {
			NSC_LOG_ERROR_STD("Failed to parse list");
		}
	}
	return ret;
}
py::list script_wrapper::convert(const std::list<std::wstring> &lst) {
	py::list ret;
	BOOST_FOREACH(const std::wstring &s, lst) {
		ret.append(utf8::cvt<std::string>(s));
	}
	return ret;
}
py::list script_wrapper::convert(const std::vector<std::wstring> &lst) {
	py::list ret;
	BOOST_FOREACH(const std::wstring &s, lst) {
		ret.append(s);
	}
	return ret;
}
py::list script_wrapper::convert(const std::list<std::string> &lst) {
	py::list ret;
	BOOST_FOREACH(const std::string &s, lst) {
		ret.append(s);
	}
	return ret;
}


void script_wrapper::log_msg(object x) {
	std::string msg = pystr(x);
	{
		thread_unlocker unlocker;
		NSC_LOG_MESSAGE(msg);
	}
}
void script_wrapper::log_error(object x) {
	std::string msg = pystr(x);
	{
		thread_unlocker unlocker;
		NSC_LOG_ERROR_STD(msg);
	}
}
void script_wrapper::log_debug(object x) {
	std::string msg = pystr(x);
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


void script_wrapper::log_exception() {
	try {
		PyErr_Print();
		boost::python::object sys(boost::python::handle<>(PyImport_ImportModule("sys")));
		boost::python::object err = sys.attr("stderr");
		std::string err_text = boost::python::extract<std::string>(err.attr("getvalue")());
		NSC_LOG_ERROR_STD(err_text);
		PyErr_Clear();
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to parse error: ", e);
		PyErr_Clear();
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to parse python error");
		PyErr_Clear();
	}
}

boost::shared_ptr<script_wrapper::function_wrapper> script_wrapper::function_wrapper::create(unsigned int plugin_id) {
	return boost::shared_ptr<function_wrapper>(new function_wrapper(get_core(), plugin_id));
}

void script_wrapper::function_wrapper::subscribe_simple_function(std::string channel, PyObject* callable) {
	try {
		nscapi::core_helper ch(core, plugin_id);
		ch.register_channel(channel);
		boost::python::handle<> h(boost::python::borrowed(callable));
		//return boost::python::object o(h);
		functions::get()->simple_handler[channel] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + channel + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + channel);
	}
}
void script_wrapper::function_wrapper::subscribe_function(std::string channel, PyObject* callable) {
	try {
		nscapi::core_helper ch(core, plugin_id);
		ch.register_channel(channel);
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->normal_handler[channel] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + channel + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + channel);
	}
}


void script_wrapper::function_wrapper::register_simple_function(std::string name, PyObject* callable, std::string desc) {
	try {
		nscapi::core_helper ch(core, plugin_id);
		ch.register_command(name, desc);
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->simple_functions[name] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + name + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + name);
	}
}
void script_wrapper::function_wrapper::register_function(std::string name, PyObject* callable, std::string desc) {
	try {
		nscapi::core_helper ch(core, plugin_id);
		ch.register_command(name, desc);
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->normal_functions[name] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + name + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + name);
	}
}
void script_wrapper::function_wrapper::register_simple_cmdline(std::string name, PyObject* callable) {
	try {
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->simple_cmdline[name] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + name + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + name);
	}
}
void script_wrapper::function_wrapper::register_cmdline(std::string name, PyObject* callable) {
	try {
		boost::python::handle<> h(boost::python::borrowed(callable));
		functions::get()->normal_cmdline[name] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + name + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + name);
	}
}

tuple script_wrapper::function_wrapper::query(std::string request) {
	try {
		std::string response;
		NSCAPI::errorReturn ret = core->registry_query(request, response);
		return boost::python::make_tuple(ret, response);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed: ", e);
		return boost::python::make_tuple(false,utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
		return boost::python::make_tuple(false,std::wstring());
	}
}

int script_wrapper::function_wrapper::handle_query(const std::string cmd, const std::string &request, std::string &response) const {
	try {
		functions::function_map_type::iterator it = functions::get()->normal_functions.find(cmd);
		if (it == functions::get()->normal_functions.end()) {
			NSC_LOG_ERROR_STD("Failed to find python function: " + cmd);
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
		NSC_LOG_ERROR_EX(cmd);
		return NSCAPI::returnUNKNOWN;
	}
}

int script_wrapper::function_wrapper::handle_simple_query(const std::string cmd, std::list<std::string> arguments, std::string &msg, std::string &perf) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_functions.find(cmd);
		if (it == functions::get()->simple_functions.end()) {
			NSC_LOG_ERROR_STD("Failed to find python function: " + cmd);
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;

			try {
				py::list l;
				BOOST_FOREACH(std::string a, arguments) {
					l.append(a);
				}
				object ret = boost::python::call<object>(boost::python::object(it->second).ptr(), l);
				if (ret.ptr() == Py_None) {
					msg = "None";
					return NSCAPI::returnUNKNOWN;
				}
				int ret_code = NSCAPI::returnUNKNOWN;
				if (len(ret) > 0)
					ret_code = extract<int>(ret[0]);
				if (len(ret) > 1)
					msg = pystr(ret[1]);
				if (len(ret) > 2)
					perf = pystr(ret[2]);
				return ret_code;
			} catch( error_already_set e) {
				log_exception();
				msg = "Exception in: " + cmd;
				return NSCAPI::returnUNKNOWN;
			}
		}
	} catch(const std::exception &e) {
		msg = "Exception in " + cmd + ": " + e.what();
		return NSCAPI::returnUNKNOWN;
	} catch(...) {
		msg = "Exception in " + cmd;
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
			NSC_LOG_ERROR_STD("Failed to find python function: " + cmd);
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				tuple ret = boost::python::call<tuple>(boost::python::object(it->second).ptr(), cmd, request);
				if (ret.ptr() == Py_None) {
					return NSCAPI::hasFailed;
				}
				int ret_code = NSCAPI::hasFailed;
				if (len(ret) > 0)
					ret_code = extract<int>(ret[0]);
				if (len(ret) > 1)
					response = pystr(ret[1]);
				return ret_code;
			} catch( error_already_set e) {
				log_exception();
				return NSCAPI::hasFailed;
			}
		}
	} catch(const std::exception &e) {
		NSC_LOG_ERROR_EXR(cmd, e);
		return NSCAPI::hasFailed;
	} catch(...) {
		NSC_LOG_ERROR_EX(cmd);
		return NSCAPI::hasFailed;
	}
}

int script_wrapper::function_wrapper::handle_simple_exec(const std::string cmd, std::list<std::string> arguments, std::string &result) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_cmdline.find(cmd);
		if (it == functions::get()->simple_cmdline.end()) {
			result = "Failed to find python function: " + cmd;
			NSC_LOG_ERROR_STD(result);
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				tuple ret = boost::python::call<tuple>(boost::python::object(it->second).ptr(), convert(arguments));
				if (ret.ptr() == Py_None) {
					result = "None";
					return NSCAPI::returnUNKNOWN;
				}
				int ret_code = NSCAPI::returnUNKNOWN;
				if (len(ret) > 0)
					ret_code = extract<int>(ret[0]);
				if (len(ret) > 1)
					result = extract<std::string>(ret[1]);
				return ret_code;
			} catch( error_already_set e) {
				log_exception();
				result = "Exception in: " + cmd;
				return NSCAPI::returnUNKNOWN;
			}
		}
	} catch(const std::exception &e) {
		result = "Exception in " + cmd + ": " + e.what();
		return NSCAPI::returnUNKNOWN;
	} catch(...) {
		result = "Exception in " + cmd;
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
			NSC_LOG_ERROR_STD("Failed to find python handler: " + channel);
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
		NSC_LOG_ERROR_EXR(channel, e);
		return NSCAPI::returnUNKNOWN;
	} catch(...) {
		NSC_LOG_ERROR_EX(channel);
		return NSCAPI::returnUNKNOWN;
	}
}
int script_wrapper::function_wrapper::handle_simple_message(const std::string channel, const std::string source, const std::string command, const int code, const std::string &msg, const std::string &perf) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_handler.find(channel);
		if (it == functions::get()->simple_handler.end()) {
			NSC_LOG_ERROR_STD("Failed to find python handler: " + channel);
			return NSCAPI::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				object ret = boost::python::call<object>(boost::python::object(it->second).ptr(), channel, source, command, nagios_return_to_py(code), pystr(msg), perf);
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
		NSC_LOG_ERROR_EXR(channel, e);
		return NSCAPI::hasFailed;
	} catch(...) {
		NSC_LOG_ERROR_EX(channel);
		return NSCAPI::hasFailed;
	}
}






bool script_wrapper::function_wrapper::has_cmdline(const std::string command) {
	return functions::get()->normal_cmdline.find(command) != functions::get()->normal_cmdline.end();
}
bool script_wrapper::function_wrapper::has_simple_cmdline(const std::string command) {
	return functions::get()->simple_cmdline.find(command) != functions::get()->simple_cmdline.end();
}

std::string script_wrapper::function_wrapper::get_commands() {
	std::string str;
	BOOST_FOREACH(const functions::function_map_type::value_type& i, functions::get()->normal_functions) {
		strEx::append_list(str, i.first, ", ");
	}
	BOOST_FOREACH(const functions::function_map_type::value_type& i, functions::get()->simple_functions) {
		strEx::append_list(str, i.first, ", ");
	}
	return str;
}

//////////////////////////////////////////////////////////////////////////
// Callouts from python into NSClient++
//
boost::shared_ptr<script_wrapper::command_wrapper> script_wrapper::command_wrapper::create(unsigned int plugin_id) {
	return boost::shared_ptr<command_wrapper>(new command_wrapper(get_core(), plugin_id));
}

tuple script_wrapper::command_wrapper::simple_submit(std::string channel, std::string command, status code, std::string message, std::string perf) {
	NSCAPI::nagiosReturn c = py_to_nagios_return(code);
	std::string resp;
	nscapi::core_helper ch(core, plugin_id);
	bool ret = false;
	{
		thread_unlocker unlocker;
		ret = ch.submit_simple_message(channel, "", "", command, c, message, perf, resp);
	}
	return boost::python::make_tuple(ret, resp);
}
tuple script_wrapper::command_wrapper::submit(std::string channel, std::string request) {
	std::string response;
	int ret = 0;
	try {
		thread_unlocker unlocker;
		ret = core->submit_message(channel, request, response);
	} catch (const std::exception &e) {
		return boost::python::make_tuple(false,std::string(e.what()));
	} catch (...) {
		return boost::python::make_tuple(false,std::string("Failed to submit message"));
	}
	std::string err;
	nscapi::protobuf::functions::parse_simple_submit_response(response, err);
	return boost::python::make_tuple(ret==NSCAPI::isSuccess,err);
}

bool script_wrapper::command_wrapper::reload(std::string module) {
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = core->reload(module);
	}
	return ret==NSCAPI::isSuccess;
}

std::string script_wrapper::command_wrapper::expand_path(std::string aPath) {
	thread_unlocker unlocker;
	return core->expand_path(aPath);
}

tuple script_wrapper::command_wrapper::simple_query(std::string command, py::list args) {
	std::string msg, perf;
	const std::list<std::string> arguments = convert(args);
	nscapi::core_helper ch(core, plugin_id);
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = ch.simple_query(command, arguments, msg, perf);
	}
	return boost::python::make_tuple(nagios_return_to_py(ret), msg, perf);
}
tuple script_wrapper::command_wrapper::query(std::string command, std::string request) {
	std::string response;
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = core->query(request, response);
	}
	return boost::python::make_tuple(ret,response);
}

tuple script_wrapper::command_wrapper::simple_exec(std::string target, std::string command, py::list args) {
	try {
		std::list<std::string> result;
		int ret = 0;
		nscapi::core_helper ch(core, plugin_id);
		const std::list<std::string> arguments = convert(args);
		{
			thread_unlocker unlocker;
			ret = ch.exec_simple_command(target, command, arguments, result);
		}
		return make_tuple(ret, convert(result));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to execute " + command, e);
		return boost::python::make_tuple(false,utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to execute " + command);
		return boost::python::make_tuple(false,utf8::cvt<std::wstring>(command));
	}
}
tuple script_wrapper::command_wrapper::exec(std::string target, std::string request) {
	try {
		std::string response;
		int ret = 0;
		{
			thread_unlocker unlocker;
			ret = core->exec_command(target, request, response);
		}
		return boost::python::make_tuple(ret, response);
	} catch (const std::exception &e) {
		return boost::python::make_tuple(false,utf8::utf8_from_native(e.what()));
	} catch (...) {
		return boost::python::make_tuple(false,"Failed to execute");
	}
}

boost::shared_ptr<script_wrapper::settings_wrapper> script_wrapper::settings_wrapper::create(unsigned int plugin_id) {
	return boost::shared_ptr<settings_wrapper>(new settings_wrapper(get_core(), plugin_id));
}

std::string script_wrapper::settings_wrapper::get_string(std::string path, std::string key, std::string def) {
	return settings.get_string(path, key, def);
}
void script_wrapper::settings_wrapper::set_string(std::string path, std::string key, std::string value) {
	settings.set_string(path, key, value);
}
bool script_wrapper::settings_wrapper::get_bool(std::string path, std::string key, bool def) {
	return settings.get_bool(path, key, def);
}
void script_wrapper::settings_wrapper::set_bool(std::string path, std::string key, bool value) {
	settings.set_bool(path, key, value);
}
int script_wrapper::settings_wrapper::get_int(std::string path, std::string key, int def) {
	return settings.get_int(path, key, def);
}
void script_wrapper::settings_wrapper::set_int(std::string path, std::string key, int value) {
	settings.set_int(path, key, value);
}
std::list<std::string> script_wrapper::settings_wrapper::get_section(std::string path) {
	return settings.get_keys(path);
}
void script_wrapper::settings_wrapper::save() {
	settings.save();
}

NSCAPI::settings_type script_wrapper::settings_wrapper::get_type(std::string stype) {
	if (stype == "string" || stype == "str" || stype == "s")
		return NSCAPI::key_string;
	if (stype == "integer" || stype == "int" || stype == "i")
		return NSCAPI::key_integer;
	if (stype == "bool" || stype == "b")
		return NSCAPI::key_bool;
	NSC_LOG_ERROR_STD("Invalid settings type");
	return NSCAPI::key_string;
}
void script_wrapper::settings_wrapper::settings_register_key(std::string path, std::string key, std::string stype, std::string title, std::string description, std::string defaultValue) {
	NSCAPI::settings_type type = get_type(stype);
	settings.register_key(path, key, type, title, description, defaultValue, false, false);
}
void script_wrapper::settings_wrapper::settings_register_path(std::string path, std::string title, std::string description) {
	settings.register_path(path, title, description, false, false);
}
tuple script_wrapper::settings_wrapper::query(std::string request) {
	try {
		std::string response;
		NSCAPI::errorReturn ret = core->settings_query(request, response);
		return boost::python::make_tuple(ret, response);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed", e);
		return boost::python::make_tuple(false,utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
		return boost::python::make_tuple(false,std::wstring());
	}
}
