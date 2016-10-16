#pragma once

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
#include <settings/client/settings_client_interface.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace settings_client {
	class settings_proxy : public nscapi::settings_helper::settings_impl_interface {
	private:
		settings::settings_handler_impl* core_;

	public:
		settings_proxy(settings::settings_handler_impl* core) : core_(core) {}

		typedef std::list<std::string> string_list;


		inline settings::settings_core* get_core() {
			return core_;
		}
		inline settings::instance_ptr get_impl() {
			return core_->get();
		}
		inline settings::settings_handler_impl* get_handler() {
			return core_;
		}
		virtual void register_path(std::string path, std::string title, std::string description, bool advanced, bool is_sample) {
			get_core()->register_path(0xffff, path, title, description, advanced, is_sample);
		}

		virtual void register_key(std::string path, std::string key, int type, std::string title, std::string description, std::string defValue, bool advanced, bool is_sample) {
			get_core()->register_key(0xffff, path, key, static_cast<settings::settings_core::key_type>(type), title, description, defValue, advanced, is_sample);
		}
		virtual void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) {}


		virtual std::string get_string(std::string path, std::string key, std::string def) {
			return get_impl()->get_string(path, key, def);
		}
		virtual void set_string(std::string path, std::string key, std::string value) {
			get_impl()->set_string(path, key, value);
		}
		virtual int get_int(std::string path, std::string key, int def) {
			return get_impl()->get_int(path, key, def);
		}
		virtual void set_int(std::string path, std::string key, int value) {
			get_impl()->set_int(path, key, value);
		}
		virtual bool get_bool(std::string path, std::string key, bool def) {
			return get_impl()->get_bool(path, key, def);
		}
		virtual void set_bool(std::string path, std::string key, bool value) {
			get_impl()->set_bool(path, key, value);
		}

		virtual string_list get_sections(std::string path) {
			return get_impl()->get_sections(path);
		}
		virtual string_list get_keys(std::string path) {
			return get_impl()->get_keys(path);
		}
		virtual std::string expand_path(std::string key) {
			return get_handler()->expand_path(key);
		}

		virtual void err(const char* file, int line, std::string message) {
			get_core()->get_logger()->error("settings",file, line, message);
		}
		virtual void warn(const char* file, int line, std::string message) {
			get_core()->get_logger()->warning("settings",file, line, message);
		}
		virtual void info(const char* file, int line, std::string message)  {
			get_core()->get_logger()->info("settings",file, line, message);
		}
		virtual void debug(const char* file, int line, std::string message)  {
			get_core()->get_logger()->debug("settings",file, line, message);
		}
	};
}