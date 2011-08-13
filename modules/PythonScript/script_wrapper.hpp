#pragma once

#include <boost/python.hpp>

namespace script_wrapper {
	using namespace boost::python;
	

	enum status {
		OK = NSCAPI::returnOK, 
		WARN = NSCAPI::returnWARN, 
		CRIT = NSCAPI::returnCRIT, 
		UNKNOWN = NSCAPI::returnUNKNOWN, 
	};

	void log_exception();
	void log_msg(std::wstring x);

	struct functions {
		typedef std::map<std::string,PyObject*> function_map_type;
		function_map_type simple_functions;
		function_map_type normal_functions;

		function_map_type simple_cmdline;
		function_map_type normal_cmdline;


		static boost::shared_ptr<functions> instance;
		static boost::shared_ptr<functions> get() {
			if (!instance)
				instance = boost::shared_ptr<functions>(new functions());
			return instance;
		}
		static void destroy() {
			instance.reset();
		}
	};

	struct function_wrapper {
	private:
		nscapi::core_wrapper* core;
		function_wrapper() {}
	public:
		function_wrapper(const function_wrapper &other) : core(other.core) {}
		function_wrapper& operator=(const function_wrapper &other) {
			core = other.core;
			return *this;
		}
		function_wrapper(nscapi::core_wrapper* core) : core(core) {}
		typedef std::map<std::string,PyObject*> function_map_type;
		//function_map_type simple_functions;
		//function_map_type functions;
		typedef boost::python::tuple simple_return;


		static boost::shared_ptr<function_wrapper> create() {
			return boost::shared_ptr<function_wrapper>(new function_wrapper(nscapi::plugin_singleton->get_core()));
		}

		void register_simple_cmdline(std::string name, PyObject* callable);
		void register_cmdline(std::string name, PyObject* callable);
		void register_simple_function(std::string name, PyObject* callable, std::string desc);
		void register_function(std::string name, PyObject* callable, std::string desc);
		void subscribe_function() {}
		void subscribe_simple_function() {}
		int exec_simple(const std::string wcmd, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) const;
		int exec(const std::string wcmd, const std::string &request, std::string &response) const;
		bool has_function(const std::string command);
		bool has_simple(const std::string command);

		int exec_simple_cmdline(const std::string wcmd, std::list<std::wstring> arguments, std::wstring &result) const;
		int exec_cmdline(const std::string wcmd, const std::string &request, std::string &response) const;
		bool has_cmdline(const std::string command);
		bool has_simple_cmdline(const std::string command);

		std::wstring get_commands();
	};
	struct command_wrapper {
	private:
		nscapi::core_wrapper* core;
	public:
		command_wrapper() : core(NULL) {}
		command_wrapper(const command_wrapper &other) : core(other.core) {}
		command_wrapper& operator=(const command_wrapper &other) {
			core = other.core;
			return *this;
		}
		command_wrapper(nscapi::core_wrapper* core) : core(core) {}

	public:
		static boost::shared_ptr<command_wrapper> create() {
			return boost::shared_ptr<command_wrapper>(new command_wrapper(nscapi::plugin_singleton->get_core()));
		}

		std::list<std::wstring> convert(boost::python::list lst);
		tuple simple_query(std::string command, boost::python::list args);
		tuple query(std::string command, std::string request);
		void simple_exec() {}
		void exec() {}
		void simple_submit() {}
		void submit() {}
	};

	struct settings_wrapper {
	private:
		nscapi::core_wrapper* core;
	public:
		settings_wrapper() : core(NULL) {}
		settings_wrapper(const settings_wrapper &other) : core(other.core) {}
		settings_wrapper& operator=(const settings_wrapper &other) {
			core = other.core;
			return *this;
		}
		settings_wrapper(nscapi::core_wrapper* core) : core(core) {}

	public:
		static boost::shared_ptr<settings_wrapper> create() {
			return boost::shared_ptr<settings_wrapper>(new settings_wrapper(nscapi::plugin_singleton->get_core()));
		}
		

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
	};


	class PyInitializer {
	public:  PyInitializer()  { Py_Initialize(); }  
			 ~PyInitializer() { Py_Finalize(); }
	private:  
		PyInitializer(const PyInitializer &);  
		PyInitializer & operator=(const PyInitializer &);
	};




}
