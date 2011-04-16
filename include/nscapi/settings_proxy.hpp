#pragma once

#include <settings/client/settings_client_interface.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
	class settings_proxy : public nscapi::settings_helper::settings_impl_interface {
	private:
		nscapi::core_wrapper* core_;

	public:
		settings_proxy(nscapi::core_wrapper* core) : core_(core) {}

		typedef std::list<std::wstring> string_list;

		virtual void register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced) {
			core_->settings_register_path(path, title, description, advanced);
		}

		virtual void register_key(std::wstring path, std::wstring key, int type, std::wstring title, std::wstring description, std::wstring defValue, bool advanced) {
			core_->settings_register_key(path, key, type, title, description, defValue, advanced);
		}

		virtual std::wstring get_string(std::wstring path, std::wstring key, std::wstring def) {
			return core_->getSettingsString(path, key, def);
		}
		virtual void set_string(std::wstring path, std::wstring key, std::wstring value) {
			core_->SetSettingsString(path, key, value);
		}
		virtual int get_int(std::wstring path, std::wstring key, int def) {
			return core_->getSettingsInt(path, key, def);
		}
		virtual void set_int(std::wstring path, std::wstring key, int value) {
			core_->SetSettingsInt(path, key, value);
		}
		virtual bool get_bool(std::wstring path, std::wstring key, bool def) {
			return core_->getSettingsBool(path, key, def);
		}
		virtual void set_bool(std::wstring path, std::wstring key, bool value) {
			core_->SetSettingsInt(path, key, value);
		}

		virtual string_list get_sections(std::wstring path) {
			return core_->getSettingsSection(path);
		}
		virtual string_list get_keys(std::wstring path) {
			return core_->getSettingsSection(path);
		}
		virtual std::wstring expand_path(std::wstring key) {
			return core_->expand_path(key);
		}

		virtual void err(std::string file, int line, std::wstring message) {
			core_->Message(NSCAPI::error, file, line, message);
		}
		virtual void warn(std::string file, int line, std::wstring message) {
			core_->Message(NSCAPI::warning, file, line, message);
		}
		virtual void info(std::string file, int line, std::wstring message)  {
			core_->Message(NSCAPI::log, file, line, message);
		}
		virtual void debug(std::string file, int line, std::wstring message)  {
			core_->Message(NSCAPI::debug, file, line, message);
		}
	};
}