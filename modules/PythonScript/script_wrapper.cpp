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

#include "script_wrapper.hpp"
#include "PythonScript.h"

#include <nscapi/functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>

#include <boost/thread.hpp>

namespace py = boost::python;

boost::shared_ptr<script_wrapper::functions> script_wrapper::functions::instance;

nscapi::core_wrapper* get_core() {
	return  nscapi::plugin_singleton->get_core();
}

script_wrapper::status script_wrapper::nagios_return_to_py(int code) {
	if (code == NSCAPI::query_return_codes::returnOK)
		return OK;
	if (code == NSCAPI::query_return_codes::returnWARN)
		return WARN;
	if (code == NSCAPI::query_return_codes::returnCRIT)
		return CRIT;
	if (code == NSCAPI::query_return_codes::returnUNKNOWN)
		return UNKNOWN;
	NSC_LOG_ERROR_STD("Invalid return code: " + ::str::xtos(code));
	return UNKNOWN;
}
int script_wrapper::py_to_nagios_return(status code) {
	NSCAPI::nagiosReturn c = NSCAPI::query_return_codes::returnUNKNOWN;
	if (code == OK)
		return NSCAPI::query_return_codes::returnOK;
	if (code == WARN)
		return NSCAPI::query_return_codes::returnWARN;
	if (code == CRIT)
		return NSCAPI::query_return_codes::returnCRIT;
	if (code == UNKNOWN)
		return NSCAPI::query_return_codes::returnUNKNOWN;
	NSC_LOG_ERROR_STD("Invalid return code: " + ::str::xtos(c));
	return NSCAPI::query_return_codes::returnUNKNOWN;
}

std::string pystr(py::object o) {
	try {
		if (o.ptr() == Py_None)
			return "";
		if (PyUnicode_Check(o.ptr())) {
			std::string s = PyBytes_AsString(PyUnicode_AsEncodedString(o.ptr(), "utf-8", "Error"));
			return s;
		}
		return py::extract<std::string>(o);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to convert python string: ", e);
		return "Unable to convert python string";
	} catch (...) {
		try {
			py::object type = o.attr("__class__");
			std::string stype = py::extract<std::string>(type);
			NSC_LOG_ERROR("Failed to convert " + stype + " to string");
		} catch (...) {
			NSC_LOG_ERROR("Failed to convert UNKNOWN to string");
		}
		return "Unable to convert python string ";
	}
}
std::string pystr(py::api::object_item o) {
	try {
		py::object po = o;
		return pystr(po);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to convert python string: ", e);
		return "Unable to convert python string";
	} catch (...) {
		NSC_LOG_ERROR("Failed to convert python string");
		return "Unable to convert python string";
	}
}

py::object pystr(std::string str) {
	return py::object(py::handle<>(PyUnicode_FromString(str.c_str())));
}

std::list<std::string> script_wrapper::convert(py::list lst) {
	std::list<std::string> ret;
	for (int i = 0; i < len(lst); i++) {
		try {
			py::extract<std::string> es(lst[i]);
			py::extract<long long> ei(lst[i]);
			if (es.check())
				ret.push_back(es());
			else if (ei.check())
				ret.push_back(::str::xtos(ei()));
			else
				NSC_LOG_ERROR_STD("Failed to convert object in list");
		} catch (py::error_already_set e) {
			log_exception();
		} catch (...) {
			NSC_LOG_ERROR_STD("Failed to parse list");
		}
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

void script_wrapper::log_msg(py::object x) {
	std::string msg = pystr(x);
	{
		thread_unlocker unlocker;
		NSC_LOG_MESSAGE(msg);
	}
}
void script_wrapper::log_error(py::object x) {
	std::string msg = pystr(x);
	{
		thread_unlocker unlocker;
		NSC_LOG_ERROR_STD(msg);
	}
}
void script_wrapper::log_debug(py::object x) {
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
		py::object sys(py::handle<>(PyImport_ImportModule("sys")));
		py::object err = sys.attr("stderr");
		std::string err_text = py::extract<std::string>(err.attr("getvalue")());
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
		py::handle<> h(py::borrowed(callable));
		//return py::object o(h);
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
		py::handle<> h(py::borrowed(callable));
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
		py::handle<> h(py::borrowed(callable));
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
		py::handle<> h(py::borrowed(callable));
		functions::get()->normal_functions[name] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + name + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + name);
	}
}
void script_wrapper::function_wrapper::register_simple_cmdline(std::string name, PyObject* callable) {
	try {
		py::handle<> h(py::borrowed(callable));
		functions::get()->simple_cmdline[name] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + name + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + name);
	}
}
void script_wrapper::function_wrapper::register_cmdline(std::string name, PyObject* callable) {
	try {
		py::handle<> h(py::borrowed(callable));
		functions::get()->normal_cmdline[name] = h;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register " + name + ": ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to register " + name);
	}
}

py::tuple script_wrapper::function_wrapper::query(std::string request) {
	try {
		std::string response;
		NSCAPI::errorReturn ret = core->registry_query(request, response);
		return py::make_tuple(ret, response);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed: ", e);
		return py::make_tuple(false, utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
		return py::make_tuple(false, "");
	}
}
void script_wrapper::function_wrapper::register_submit_metrics(PyObject* callable) {
	try {
		py::handle<> h(py::borrowed(callable));
		functions::get()->submit_metrics.push_back(h);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed: ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
	}
}
void script_wrapper::function_wrapper::register_fetch_metrics(PyObject* callable) {
	try {
		py::handle<> h(py::borrowed(callable));
		functions::get()->fetch_metrics.push_back(h);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed: ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
	}
}
py::tuple script_wrapper::function_wrapper::register_event_pb(std::string event, PyObject* callable) {
	try {
		nscapi::core_helper ch(core, plugin_id);
		ch.register_event(event);
		py::handle<> h(py::borrowed(callable));
		functions::get()->normal_handler[event] = h;
		return py::make_tuple(true, "");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed: ", e);
		return py::make_tuple(false, utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
		return py::make_tuple(false, std::string());
	}
}

py::tuple script_wrapper::function_wrapper::register_event(std::string event, PyObject* callable) {
	try {
		nscapi::core_helper ch(core, plugin_id);
		ch.register_event(event);
		py::handle<> h(py::borrowed(callable));
		functions::get()->simple_handler[event] = h;
		return py::make_tuple(true, "");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed: ", e);
		return py::make_tuple(false, utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
		return py::make_tuple(false, std::string());
	}
}

int script_wrapper::function_wrapper::handle_query(const std::string cmd, const std::string &request, std::string &response) const {
	try {
		functions::function_map_type::iterator it = functions::get()->normal_functions.find(cmd);
		if (it == functions::get()->normal_functions.end()) {
			NSC_LOG_ERROR_STD("Failed to find python function: " + cmd);
			return NSCAPI::cmd_return_codes::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				py::tuple ret = py::call<py::tuple>(py::object(it->second).ptr(), cmd, request);
				if (ret.ptr() == Py_None) {
					return NSCAPI::query_return_codes::returnUNKNOWN;
				}
				int ret_code = NSCAPI::query_return_codes::returnUNKNOWN;
				if (py::len(ret) > 0)
					ret_code = py::extract<int>(ret[0]);
				if (py::len(ret) > 1)
					response = py::extract<std::string>(ret[1]);
				return ret_code;
			} catch (py::error_already_set e) {
				log_exception();
				return NSCAPI::query_return_codes::returnUNKNOWN;
			}
		}
	} catch (...) {
		NSC_LOG_ERROR_EX(cmd);
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
}

int script_wrapper::function_wrapper::handle_simple_query(const std::string cmd, std::list<std::string> arguments, std::string &msg, std::string &perf) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_functions.find(cmd);
		if (it == functions::get()->simple_functions.end()) {
			NSC_LOG_ERROR_STD("Failed to find python function: " + cmd);
			return NSCAPI::cmd_return_codes::returnIgnored;
		}
		{
			thread_locker locker;

			try {
				py::list l;
				BOOST_FOREACH(std::string a, arguments) {
					l.append(a);
				}
				py::object ret = py::call<py::object>(py::object(it->second).ptr(), l);
				if (ret.ptr() == Py_None) {
					msg = "None";
					return NSCAPI::query_return_codes::returnUNKNOWN;
				}
				int ret_code = NSCAPI::query_return_codes::returnUNKNOWN;
				if (len(ret) > 0) {
					ret_code = py::extract<int>(ret[0]);
				}
				if (len(ret) > 1)
					msg = pystr(ret[1]);
				if (len(ret) > 2)
					perf = pystr(ret[2]);
				return ret_code;
			} catch (py::error_already_set e) {
				log_exception();
				msg = "Exception in: " + cmd;
				return NSCAPI::query_return_codes::returnUNKNOWN;
			}
		}
	} catch (const std::exception &e) {
		msg = "Exception in " + cmd + ": " + e.what();
		return NSCAPI::query_return_codes::returnUNKNOWN;
	} catch (...) {
		msg = "Exception in " + cmd;
		return NSCAPI::query_return_codes::returnUNKNOWN;
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
			return NSCAPI::cmd_return_codes::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				py::tuple ret = py::call<py::tuple>(py::object(it->second).ptr(), cmd, request);
				if (ret.ptr() == Py_None) {
					return NSCAPI::exec_return_codes::returnERROR;
				}
				int ret_code = NSCAPI::exec_return_codes::returnERROR;
				if (py::len(ret) > 0)
					ret_code = py::extract<int>(ret[0]);
				if (py::len(ret) > 1)
					response = pystr(ret[1]);
				return ret_code;
			} catch (py::error_already_set e) {
				log_exception();
				return NSCAPI::exec_return_codes::returnERROR;
			}
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR(cmd, e);
		return NSCAPI::exec_return_codes::returnERROR;
	} catch (...) {
		NSC_LOG_ERROR_EX(cmd);
		return NSCAPI::exec_return_codes::returnERROR;
	}
}

int script_wrapper::function_wrapper::handle_simple_exec(const std::string cmd, std::list<std::string> arguments, std::string &result) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_cmdline.find(cmd);
		if (it == functions::get()->simple_cmdline.end()) {
			result = "Failed to find python function: " + cmd;
			NSC_LOG_ERROR_STD(result);
			return NSCAPI::cmd_return_codes::returnIgnored;
		}
		{
			thread_locker locker;
			try {
				py::tuple ret = py::call<py::tuple>(py::object(it->second).ptr(), convert(arguments));
				if (ret.ptr() == Py_None) {
					result = "None";
					return NSCAPI::exec_return_codes::returnERROR;
				}
				int ret_code = NSCAPI::exec_return_codes::returnERROR;
				if (py::len(ret) > 0)
					ret_code = py::extract<int>(ret[0]);
				if (py::len(ret) > 1)
					result = py::extract<std::string>(ret[1]);
				return ret_code;
			} catch (py::error_already_set e) {
				log_exception();
				result = "Exception in: " + cmd;
				return NSCAPI::exec_return_codes::returnERROR;
			}
		}
	} catch (const std::exception &e) {
		result = "Exception in " + cmd + ": " + e.what();
		return NSCAPI::exec_return_codes::returnERROR;
	} catch (...) {
		result = "Exception in " + cmd;
		return NSCAPI::exec_return_codes::returnERROR;
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
			return NSCAPI::api_return_codes::hasFailed;
		}
		{
			thread_locker locker;
			int ret_code = NSCAPI::api_return_codes::hasFailed;
			try {
				py::object ret = py::call<py::object>(py::object(it->second).ptr(), channel, request);
				if (ret.ptr() == Py_None) {
					return NSCAPI::api_return_codes::hasFailed;
				}
				if (py::len(ret) > 0)
					ret_code = py::extract<bool>(ret[0]) ? NSCAPI::api_return_codes::isSuccess : NSCAPI::api_return_codes::hasFailed;
				if (py::len(ret) > 1)
					response = py::extract<std::string>(ret[1]);
			} catch (py::error_already_set e) {
				log_exception();
				return NSCAPI::api_return_codes::hasFailed;
			}
			return ret_code;
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR(channel, e);
		return NSCAPI::api_return_codes::hasFailed;
	} catch (...) {
		NSC_LOG_ERROR_EX(channel);
		return NSCAPI::api_return_codes::hasFailed;
	}
}
int script_wrapper::function_wrapper::handle_simple_message(const std::string channel, const std::string source, const std::string command, const int code, const std::string &msg, const std::string &perf) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_handler.find(channel);
		if (it == functions::get()->simple_handler.end()) {
			NSC_LOG_ERROR_STD("Failed to find python handler: " + channel);
			return NSCAPI::api_return_codes::hasFailed;
		}
		{
			thread_locker locker;
			try {
				py::object ret = py::call<py::object>(py::object(it->second).ptr(), channel, source, command, nagios_return_to_py(code), pystr(msg), perf);
				int ret_code = NSCAPI::api_return_codes::hasFailed;
				if (ret.ptr() == Py_None) {
					ret_code = NSCAPI::api_return_codes::isSuccess;
				} else {
					ret_code = py::extract<bool>(ret) ? NSCAPI::api_return_codes::isSuccess : NSCAPI::api_return_codes::hasFailed;
				}
				return ret_code;
			} catch (py::error_already_set e) {
				log_exception();
				return NSCAPI::api_return_codes::hasFailed;
			}
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR(channel, e);
		return NSCAPI::api_return_codes::hasFailed;
	} catch (...) {
		NSC_LOG_ERROR_EX(channel);
		return NSCAPI::api_return_codes::hasFailed;
	}
}



bool script_wrapper::function_wrapper::has_event_handler(const std::string channel) {
	return functions::get()->normal_handler.find(channel) != functions::get()->normal_handler.end();
}
bool script_wrapper::function_wrapper::has_simple_event_handler(const std::string channel) {
	return functions::get()->simple_handler.find(channel) != functions::get()->simple_handler.end();
}

void script_wrapper::function_wrapper::on_event(const std::string event, const std::string &request) const {
	try {
		functions::function_map_type::iterator it = functions::get()->normal_handler.find(event);
		if (it == functions::get()->normal_handler.end()) {
			NSC_LOG_ERROR_STD("Failed to find python handler: " + event);
		}
		{
			thread_locker locker;
			try {
				py::call<py::object>(py::object(it->second).ptr(), event, request);
			} catch (py::error_already_set e) {
				log_exception();
			}
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR(event, e);
	} catch (...) {
		NSC_LOG_ERROR_EX(event);
	}
}
void script_wrapper::function_wrapper::on_simple_event(const std::string event, const py::dict &data) const {
	try {
		functions::function_map_type::iterator it = functions::get()->simple_handler.find(event);
		if (it == functions::get()->simple_handler.end()) {
			NSC_LOG_ERROR_STD("Failed to find python handler: " + event);
		}
		{
			thread_locker locker;
			try {
				py::call<void>(py::object(it->second).ptr(), event, data);
			} catch (py::error_already_set e) {
				log_exception();
			}
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR(event, e);
	} catch (...) {
		NSC_LOG_ERROR_EX(event);
	}
}


void build_metrics(py::dict &metrics, const Plugin::Common::MetricsBundle &b, const std::string &path) {
	std::string p = "";
	if (!path.empty())
		p += path + ".";
	p += b.key();

	BOOST_FOREACH(const Plugin::Common::MetricsBundle &b2, b.children()) {
		build_metrics(metrics, b2, p);
	}

	BOOST_FOREACH(const Plugin::Common::Metric &v, b.value()) {
		if (!v.has_value())
			continue;
		if (v.value().has_int_data())
			metrics[p + "." + v.key()] = str::xtos(v.value().int_data());
		else if (v.value().has_string_data())
			metrics[p + "." + v.key()] = v.value().string_data();
		else if (v.value().has_float_data())
			metrics[p + "." + v.key()] = str::xtos(v.value().float_data());
	}
}

void script_wrapper::function_wrapper::submit_metrics(const std::string &request) const {

	py::dict metrics;
	Plugin::MetricsMessage msg;
	msg.ParseFromString(request);
	BOOST_FOREACH(const Plugin::MetricsMessage::Response &p, msg.payload()) {
		BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, p.bundles()) {
			build_metrics(metrics, b, "");
		}
	}


	try {
		BOOST_FOREACH(functions::function_list_type::value_type &v, functions::get()->submit_metrics) {
			thread_locker locker;
			try {
				py::call<py::object>(py::object(v).ptr(), metrics, pystr(""));
			} catch (py::error_already_set e) {
				log_exception();
			}
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Submission failed", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Submission failed");
	}
}
void script_wrapper::function_wrapper::fetch_metrics(std::string &request) const {
	Plugin::MetricsMessage::Response payload;
	Plugin::Common::MetricsBundle *bundle = payload.add_bundles();
	bundle->set_key("");


	try {
		BOOST_FOREACH(functions::function_list_type::value_type &v, functions::get()->fetch_metrics) {
			thread_locker locker;
			try {
				py::object ret = py::call<py::object>(py::object(v).ptr());
#if BOOST_VERSION > 104200
				if (ret.is_none())
					continue;
#endif
				py::extract<py::dict> extracter(ret);
				if (extracter.check()) {
					py::dict dic = extracter;

					py::list keys = dic.keys();

					for (int i = 0; i < len(keys); ++i) {
						py::object curArg = dic[keys[i]];
						if (curArg) {
							Plugin::Common::Metric *v = bundle->add_value();
							v->set_key(py::extract<std::string>(keys[i]));

							py::extract<std::string> strExtr(dic[keys[i]]);
							if (strExtr.check()) {
								v->mutable_value()->set_string_data(strExtr);
								continue;
							}
							py::extract<int> intExtr(dic[keys[i]]);
							if (intExtr.check()) {
								v->mutable_value()->set_int_data(intExtr);
								continue;
							}
							py::extract<double> dblExtr(dic[keys[i]]);
							if (dblExtr.check()) {
								v->mutable_value()->set_float_data(dblExtr);
								continue;
							}
							v->mutable_value()->set_string_data("unknown type");
						}
					}
				}
			} catch (py::error_already_set e) {
				log_exception();
			}
		}
		payload.mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		request = payload.SerializeAsString();
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Submission failed", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Submission failed");
	}
}

bool script_wrapper::function_wrapper::has_submit_metrics() {
	return true;
}
bool script_wrapper::function_wrapper::has_metrics_fetcher() {
	return true;
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
		str::format::append_list(str, i.first, ", ");
	}
	BOOST_FOREACH(const functions::function_map_type::value_type& i, functions::get()->simple_functions) {
		str::format::append_list(str, i.first, ", ");
	}
	return str;
}

//////////////////////////////////////////////////////////////////////////
// Callouts from python into NSClient++
//
boost::shared_ptr<script_wrapper::command_wrapper> script_wrapper::command_wrapper::create(unsigned int plugin_id) {
	return boost::shared_ptr<command_wrapper>(new command_wrapper(get_core(), plugin_id));
}

py::tuple script_wrapper::command_wrapper::simple_submit(std::string channel, std::string command, status code, std::string message, std::string perf) {
	NSCAPI::nagiosReturn c = py_to_nagios_return(code);
	std::string resp;
	nscapi::core_helper ch(core, plugin_id);
	bool ret = false;
	{
		thread_unlocker unlocker;
		ret = ch.submit_simple_message(channel, "", "", command, c, message, perf, resp);
	}
	return py::make_tuple(ret, resp);
}
py::tuple script_wrapper::command_wrapper::submit(std::string channel, std::string request) {
	std::string response;
	int ret = 0;
	try {
		thread_unlocker unlocker;
		ret = core->submit_message(channel, request, response);
	} catch (const std::exception &e) {
		return py::make_tuple(false, std::string(e.what()));
	} catch (...) {
		return py::make_tuple(false, std::string("Failed to submit message"));
	}
	std::string err;
	nscapi::protobuf::functions::parse_simple_submit_response(response, err);
	return py::make_tuple(ret == NSCAPI::api_return_codes::isSuccess, err);
}

bool script_wrapper::command_wrapper::reload(std::string module) {
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = core->reload(module);
	}
	return ret == NSCAPI::api_return_codes::isSuccess;
}

bool script_wrapper::command_wrapper::load_module(std::string name, std::string alias) {
	int ret = 0;
	{
		thread_unlocker unlocker;
		nscapi::core_helper ch(core, plugin_id);
		ret = ch.load_module(name, alias);

	}
	return ret == NSCAPI::api_return_codes::isSuccess;
}

bool script_wrapper::command_wrapper::unload_module(std::string name) {
	int ret = 0;
	{
		thread_unlocker unlocker;
		nscapi::core_helper ch(core, plugin_id);
		ret = ch.unload_module(name);

	}
	return ret == NSCAPI::api_return_codes::isSuccess;
}

std::string script_wrapper::command_wrapper::expand_path(std::string aPath) {
	thread_unlocker unlocker;
	return core->expand_path(aPath);
}

py::tuple script_wrapper::command_wrapper::simple_query(std::string command, py::list args) {
	std::string msg, perf;
	const std::list<std::string> arguments = convert(args);
	nscapi::core_helper ch(core, plugin_id);
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = ch.simple_query(command, arguments, msg, perf, -1);
	}
	return py::make_tuple(nagios_return_to_py(ret), msg, perf);
}
py::tuple script_wrapper::command_wrapper::query(std::string command, std::string request) {
	std::string response;
	int ret = 0;
	{
		thread_unlocker unlocker;
		ret = core->query(request, response);
	}
	return py::make_tuple(ret, response);
}

py::tuple script_wrapper::command_wrapper::simple_exec(std::string target, std::string command, py::list args) {
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
		return py::make_tuple(false, utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to execute " + command);
		return py::make_tuple(false, command);
	}
}
py::tuple script_wrapper::command_wrapper::exec(std::string target, std::string request) {
	try {
		std::string response;
		int ret = 0;
		{
			thread_unlocker unlocker;
			ret = core->exec_command(target, request, response);
		}
		return py::make_tuple(ret, response);
	} catch (const std::exception &e) {
		return py::make_tuple(false, utf8::utf8_from_native(e.what()));
	} catch (...) {
		return py::make_tuple(false, "Failed to execute");
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
boost::python::list script_wrapper::settings_wrapper::get_section(std::string path) {
	return convert(settings.get_keys(path));
}
void script_wrapper::settings_wrapper::save() {
	settings.save();
}

void script_wrapper::settings_wrapper::settings_register_key(std::string path, std::string key, std::string stype, std::string title, std::string description, std::string defaultValue) {
	settings.register_key(path, key, title, description, defaultValue, false, false);
}
void script_wrapper::settings_wrapper::settings_register_path(std::string path, std::string title, std::string description) {
	settings.register_path(path, title, description, false, false);
}
py::tuple script_wrapper::settings_wrapper::query(std::string request) {
	try {
		std::string response;
		NSCAPI::errorReturn ret = core->settings_query(request, response);
		return py::make_tuple(ret, response);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Query failed", e);
		return py::make_tuple(false, utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Query failed");
		return py::make_tuple(false, std::string());
	}
}