#pragma once

#include <boost/python.hpp>
#include <boost/thread.hpp>

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>


namespace script_wrapper {
	using namespace boost::python;


	namespace thread_support {
		static bool enabled = true;
		static boost::shared_mutex mutex;

		struct log_lock {
			boost::unique_lock<boost::shared_mutex> lock;
			log_lock() : lock(thread_support::mutex, boost::get_system_time() + boost::posix_time::seconds(2)) {
				if (!lock.owns_lock())
					NSC_LOG_ERROR("Failed to get mutex: thread_locker");
			}
		};

	}

	struct thread_locker {
		PyGILState_STATE state;
		thread_locker() {
			if (thread_support::enabled)
				state = PyGILState_Ensure();
		}

		~thread_locker() {
			if (thread_support::enabled)
				PyGILState_Release(state);
		}
	};


	struct thread_unlocker {
		PyThreadState *state;
		thread_unlocker() {
			if (thread_support::enabled)
				state = PyEval_SaveThread();
		}
		~thread_unlocker() {
			if (thread_support::enabled)
				PyEval_RestoreThread(state);
		}
	};
	
	enum status {
		OK = NSCAPI::returnOK, 
		WARN = NSCAPI::returnWARN, 
		CRIT = NSCAPI::returnCRIT, 
		UNKNOWN = NSCAPI::returnUNKNOWN
	};

	status nagios_return_to_py(int code);
	int py_to_nagios_return(status code);

	void log_exception();
	void log_msg(object x);
	void log_debug(object x);
	void log_error(object x);
	void sleep(unsigned int seconds);
	//std::string get_alias();

	std::list<std::string> convert(boost::python::list lst);
	boost::python::list convert(const std::list<std::string> &lst);
	boost::python::list convert(const std::list<std::wstring> &lst);
	boost::python::list convert(const std::vector<std::wstring> &lst);


	struct functions {
		typedef std::map<std::string,boost::python::handle<> > function_map_type;
		function_map_type simple_functions;
		function_map_type normal_functions;

		function_map_type simple_cmdline;
		function_map_type normal_cmdline;

		function_map_type simple_handler;
		function_map_type normal_handler;

		static boost::shared_ptr<functions> instance;
		static boost::shared_ptr<functions> get() {
			if (!instance)
				instance = boost::shared_ptr<functions>(new functions());
			return instance;
		}
		static void destroy() {
			instance.reset();
		}
		std::list<std::string> get_commands() const {
			std::list<std::string> ret;
			BOOST_FOREACH(const function_map_type::value_type &v, simple_functions) {
				ret.push_back(v.first);
			}
			BOOST_FOREACH(const function_map_type::value_type &v, normal_functions) {
				ret.push_back(v.first);
			}
			return ret;
		}
	};

	struct function_wrapper {
	private:
		nscapi::core_wrapper* core;
		unsigned int plugin_id;
	public:
		function_wrapper(const function_wrapper &other) : core(other.core), plugin_id(other.plugin_id) {}
		function_wrapper& operator=(const function_wrapper &other) {
			core = other.core;
			plugin_id = other.plugin_id;
			return *this;
		}
		function_wrapper(nscapi::core_wrapper* core, unsigned int plugin_id) : core(core), plugin_id(plugin_id) {}
		typedef std::map<std::string,PyObject*> function_map_type;
		//typedef boost::python::tuple simple_return;


		static boost::shared_ptr<function_wrapper> create(unsigned int plugin_id);

		void register_simple_cmdline(std::string name, PyObject* callable);
		void register_cmdline(std::string name, PyObject* callable);
		void register_simple_function(std::string name, PyObject* callable, std::string desc);
		void register_function(std::string name, PyObject* callable, std::string desc);
		void subscribe_function(std::string channel, PyObject* callable);
		void subscribe_simple_function(std::string channel, PyObject* callable);
		int handle_simple_query(const std::string wcmd, std::list<std::string> arguments, std::string &msg, std::string &perf) const;
		int handle_query(const std::string wcmd, const std::string &request, std::string &response) const;
		bool has_function(const std::string command);
		bool has_simple(const std::string command);

		int handle_simple_exec(const std::string cmd, std::list<std::string> arguments, std::string &result) const;
		int handle_exec(const std::string wcmd, const std::string &request, std::string &response) const;
		bool has_cmdline(const std::string command);
		bool has_simple_cmdline(const std::string command);


		int handle_simple_message(const std::string channel, const std::string wsrc, const std::string wcmd, const int code, const std::string &msg, const std::string &perf) const;
		int handle_message(const std::string channel, const std::string &request, std::string &response) const;
		bool has_message_handler(const std::string command);
		bool has_simple_message_handler(const std::string command);

		std::string get_commands();
		tuple query(std::string request);
	};
	struct command_wrapper {
	private:
		nscapi::core_wrapper* core;
		unsigned int plugin_id;
	public:
		command_wrapper(const command_wrapper &other) : core(other.core) {}
		command_wrapper& operator=(const command_wrapper &other) {
			core = other.core;
			return *this;
		}
		command_wrapper(nscapi::core_wrapper* core, unsigned int plugin_id) : core(core), plugin_id(plugin_id) {}

	public:
		static boost::shared_ptr<command_wrapper> create(unsigned int plugin_id);

		tuple simple_query(std::string command, boost::python::list args);
		tuple query(std::string command, std::string request);
		tuple simple_exec(std::string target, std::string command, boost::python::list args);
		tuple exec(std::string target, std::string request);
		tuple simple_submit(std::string channel, std::string command, status code, std::string message, std::string perf);
		tuple submit(std::string channel, std::string request);
		bool reload(std::string module);
		std::string expand_path(std::string module);
	};

	struct settings_wrapper {
	private:
		nscapi::core_wrapper* core;
		unsigned int plugin_id;
		nscapi::settings_proxy settings;
	public:
		settings_wrapper(const settings_wrapper &other) : core(other.core), plugin_id(other.plugin_id), settings(other.plugin_id, other.core) {}
		settings_wrapper& operator=(const settings_wrapper &other) {
			core = other.core;
			plugin_id = other.plugin_id;
			settings = other.settings;
			return *this;
		}
		settings_wrapper(nscapi::core_wrapper* core, unsigned int plugin_id) : core(core), plugin_id(plugin_id), settings(plugin_id, core) {}

	public:
		static boost::shared_ptr<settings_wrapper> create(unsigned int plugin_id);

		std::string get_string(std::string path, std::string key, std::string def = "");
		void set_string(std::string path, std::string key, std::string value);
		bool get_bool(std::string path, std::string key, bool def);
		void set_bool(std::string path, std::string key, bool value);
		int get_int(std::string path, std::string key, int def);
		void set_int(std::string path, std::string key, int value);
		std::list<std::string> get_section(std::string path);
		void save();
		NSCAPI::settings_type get_type(std::string stype);
		void settings_register_key(std::string path, std::string key, std::string stype, std::string title, std::string description, std::string defaultValue);
		void settings_register_path(std::string path, std::string title, std::string description);
		tuple query(std::string request);
	};


	class PyInitializer {
	public:  PyInitializer()  { Py_Initialize(); }  
			 ~PyInitializer() { Py_Finalize(); }
	private:  
		PyInitializer(const PyInitializer &);  
		PyInitializer & operator=(const PyInitializer &);
	};




}
