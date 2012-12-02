#pragma once
#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace settings_client {
	class settings_proxy : public nscapi::settings_helper::settings_impl_interface {
	private:
		settings::settings_handler_impl* core_;

	public:
		settings_proxy(settings::settings_handler_impl* core) : core_(core) {}

		typedef std::list<std::wstring> string_list;


		inline settings::settings_core* get_core() {
			return core_;
		}
		inline settings::instance_ptr get_impl() {
			return core_->get();
		}
		inline settings::settings_handler_impl* get_handler() {
			return core_;
		}
		virtual void register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced) {
			get_core()->register_path(0xffff, path, title, description, advanced);
		}

		virtual void register_key(std::wstring path, std::wstring key, int type, std::wstring title, std::wstring description, std::wstring defValue, bool advanced) {
			get_core()->register_key(0xffff, path, key, static_cast<settings::settings_core::key_type>(type), title, description, defValue, advanced);
		}

		virtual std::wstring get_string(std::wstring path, std::wstring key, std::wstring def) {
			return get_impl()->get_string(path, key, def);
		}
		virtual void set_string(std::wstring path, std::wstring key, std::wstring value) {
			get_impl()->set_string(path, key, value);
		}
		virtual int get_int(std::wstring path, std::wstring key, int def) {
			return get_impl()->get_int(path, key, def);
		}
		virtual void set_int(std::wstring path, std::wstring key, int value) {
			get_impl()->set_int(path, key, value);
		}
		virtual bool get_bool(std::wstring path, std::wstring key, bool def) {
			return get_impl()->get_bool(path, key, def);
		}
		virtual void set_bool(std::wstring path, std::wstring key, bool value) {
			get_impl()->set_bool(path, key, value);
		}

		virtual string_list get_sections(std::wstring path) {
			return get_impl()->get_sections(path);
		}
		virtual string_list get_keys(std::wstring path) {
			return get_impl()->get_keys(path);
		}
		virtual std::wstring expand_path(std::wstring key) {
			return get_handler()->expand_path(key);
		}

		virtual void err(const char* file, int line, std::wstring message) {
			nsclient::logging::logger::get_logger()->error(_T("settings"),file, line, message);
		}
		virtual void warn(const char* file, int line, std::wstring message) {
			nsclient::logging::logger::get_logger()->warning(_T("settings"),file, line, message);
		}
		virtual void info(const char* file, int line, std::wstring message)  {
			nsclient::logging::logger::get_logger()->info(_T("settings"),file, line, message);
		}
		virtual void debug(const char* file, int line, std::wstring message)  {
			nsclient::logging::logger::get_logger()->debug(_T("settings"),file, line, message);
		}
	};
}