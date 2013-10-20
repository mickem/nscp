#pragma once

#include <list>

#include <settings/settings_core.hpp>
#include <settings/client/settings_client_interface.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
	class settings_proxy : public nscapi::settings_helper::settings_impl_interface {
	private:
		unsigned int plugin_id_;
		nscapi::core_wrapper* core_;

	public:
		settings_proxy(unsigned int plugin_id, nscapi::core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}

		typedef std::list<std::string> string_list;

		virtual void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample);
		virtual void register_key(std::string path, std::string key, int type, std::string title, std::string description, std::string defValue, bool advanced, bool sample);

		virtual std::string get_string(std::string path, std::string key, std::string def);
		virtual void set_string(std::string path, std::string key, std::string value);
		virtual int get_int(std::string path, std::string key, int def);
		virtual void set_int(std::string path, std::string key, int value);
		virtual bool get_bool(std::string path, std::string key, bool def);
		virtual void set_bool(std::string path, std::string key, bool value);
		virtual string_list get_sections(std::string path);
		virtual string_list get_keys(std::string path);
		virtual std::string expand_path(std::string key);

		virtual void err(const char* file, int line, std::string message);
		virtual void warn(const char* file, int line, std::string message);
		virtual void info(const char* file, int line, std::string message);
		virtual void debug(const char* file, int line, std::string message);
		void save(const std::string context = "");
	};
}