#include "python_script.hpp"

#include "script_wrapper.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/python.hpp>

namespace py = boost::python;

#if BOOST_VERSION == 105800
py::object fix_for_broken_exec_file(char const *filename, py::object global, py::object local) {
	char *f = const_cast<char *>(filename);
	PyObject *pyfile = PyFile_FromString(f, const_cast<char*>("r"));
	if (!pyfile) throw std::invalid_argument(std::string(filename) + " : no such file");
	py::handle<> file(pyfile);
	FILE *fs = PyFile_AsFile(file.get());
	PyObject* result = PyRun_File(fs, f, Py_file_input, global.ptr(), local.ptr());
	if (!result) py::throw_error_already_set();
	return py::object(py::detail::new_reference(result));
}
#endif

BOOST_PYTHON_MODULE(NSCP) {
	py::class_<script_wrapper::settings_wrapper, boost::shared_ptr<script_wrapper::settings_wrapper> >("Settings", py::no_init)
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
	py::class_<script_wrapper::function_wrapper, boost::shared_ptr<script_wrapper::function_wrapper> >("Registry", py::no_init)
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
	py::class_<script_wrapper::command_wrapper, boost::shared_ptr<script_wrapper::command_wrapper> >("Core", py::no_init)
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

	py::enum_<script_wrapper::status>("status")
		.value("CRITICAL", script_wrapper::CRIT)
		.value("WARNING", script_wrapper::WARN)
		.value("UNKNOWN", script_wrapper::UNKNOWN)
		.value("OK", script_wrapper::OK)
		;
	py::def("log", script_wrapper::log_msg);
	py::def("log_err", script_wrapper::log_error);
	py::def("log_deb", script_wrapper::log_debug);
	py::def("log_error", script_wrapper::log_error);
	py::def("log_debug", script_wrapper::log_debug);
	py::def("sleep", script_wrapper::sleep);
}

static bool has_init = false;


void python_script::init() {
	NSC_DEBUG_MSG("boot python");

	bool do_init = false;
	if (!has_init) {
		has_init = true;
		Py_Initialize();
		PyEval_InitThreads();
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
			} catch (py::error_already_set e) {
				script_wrapper::log_exception();
			}
		}
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("load python scripts", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("load python scripts");
	}
}

python_script::python_script(unsigned int plugin_id, const std::string base_path, const std::string plugin_alias, const std::string script_alias, const std::string script)
	: base_path(base_path)
	, plugin_id(plugin_id) {
	NSC_DEBUG_MSG_STD("Loading python script: " + script);
	_exec(script);
	NSC_DEBUG_MSG_STD("Initializing script: " + script);
	callFunction("init", plugin_id, plugin_alias, script_alias);
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
			py::object scriptFunction = py::extract<py::object>(localDict[functionName]);
			if (scriptFunction)
				scriptFunction();
			return true;
		} catch (py::error_already_set e) {
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
			py::object scriptFunction = py::extract<py::object>(localDict[functionName]);
			if (scriptFunction)
				scriptFunction(script_wrapper::convert(args));
			return true;
		} catch (py::error_already_set e) {
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
			py::object scriptFunction = py::extract<py::object>(localDict[functionName]);
			if (scriptFunction)
				scriptFunction(i1, s1, s2);
			return true;
		} catch (py::error_already_set e) {
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
			py::object main_module = py::import("__main__");
			py::dict globalDict = py::extract<py::dict>(main_module.attr("__dict__"));
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
			} catch (py::error_already_set e) {
				NSC_LOG_ERROR("Failed to setup env for script: " + scriptfile);
				script_wrapper::log_exception();
				return;
			}

#if BOOST_VERSION == 105800
			py::object ignored = fix_for_broken_exec_file(scriptfile.c_str(), localDict, localDict);
#else
			py::object ignored = exec_file(scriptfile.c_str(), localDict, localDict);
#endif
		} catch (py::error_already_set e) {
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
