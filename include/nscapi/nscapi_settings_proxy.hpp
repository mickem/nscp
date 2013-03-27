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
		/*
		inline settings::settings_core* get_core() {
			return core_;
		}
		inline settings::instance_ptr get_impl() {
			return core_->get();
		}
		inline settings::settings_handler_impl* get_handler() {
			return core_;
		}
		*/

		typedef std::list<std::string> string_list;

		virtual void register_path(std::string path, std::string title, std::string description, bool advanced);
		virtual void register_key(std::string path, std::string key, int type, std::string title, std::string description, std::string defValue, bool advanced);

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