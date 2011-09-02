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
	//std::string get_alias();

	std::list<std::wstring> convert(boost::python::list lst);
	boost::python::list convert(std::list<std::wstring> lst);


	struct functions {
		typedef std::map<std::string,PyObject*> function_map_type;
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
	};

	struct function_wrapper {
	private:
		nscapi::core_wrapper* core;
		unsigned int plugin_id;
		function_wrapper() {}
	public:
		function_wrapper(const function_wrapper &other) : core(other.core) {}
		function_wrapper& operator=(const function_wrapper &other) {
			core = other.core;
			return *this;
		}
		function_wrapper(nscapi::core_wrapper* core, unsigned int plugin_id) : core(core), plugin_id(plugin_id) {}
		typedef std::map<std::string,PyObject*> function_map_type;
		//typedef boost::python::tuple simple_return;


		static boost::shared_ptr<function_wrapper> create(unsigned int plugin_id) {
			return boost::shared_ptr<function_wrapper>(new function_wrapper(nscapi::plugin_singleton->get_core(), plugin_id));
		}

		void register_simple_cmdline(std::string name, PyObject* callable);
		void register_cmdline(std::string name, PyObject* callable);
		void register_simple_function(std::string name, PyObject* callable, std::string desc);
		void register_function(std::string name, PyObject* callable, std::string desc);
		void subscribe_function(std::string channel, PyObject* callable);
		void subscribe_simple_function(std::string channel, PyObject* callable);
		int exec_simple(const std::string wcmd, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) const;
		int exec(const std::string wcmd, const std::string &request, std::string &response) const;
		bool has_function(const std::string command);
		bool has_simple(const std::string command);

		int handle_simple_exec(const std::string wcmd, std::list<std::wstring> arguments, std::wstring &result) const;
		int handle_exec(const std::string wcmd, const std::string &request, std::string &response) const;
		bool has_cmdline(const std::string command);
		bool has_simple_cmdline(const std::string command);


		int handle_simple_message(const std::string channel, const std::string wcmd, int code, std::wstring &msg, std::wstring &perf) const;
		int handle_message(const std::string channel, const std::string wcmd, std::string &message) const;
		bool has_message_handler(const std::string command);
		bool has_simple_message_handler(const std::string command);

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

		tuple simple_query(std::string command, boost::python::list args);
		tuple query(std::string command, std::string request);
		object simple_exec(std::string command, boost::python::list args);
		tuple exec(std::string command, std::string request);
		void simple_submit(std::string channel, std::string command, status code, std::string message, std::string perf);
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
