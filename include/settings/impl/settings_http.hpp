#pragma once

#include <string>
#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <http/client.hpp>
#include <net/net.hpp>
#include <file_helpers.hpp>

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
#include <error.hpp>

namespace settings {
	class settings_http : public settings::SettingsInterfaceImpl {
	private:
		std::wstring url_;

		inline nsclient::logging::logger_interface* get_logger() const {
			return nsclient::logging::logger::get_logger();
		}

	public:
		settings_http(settings::settings_core *core, std::wstring context) : settings::SettingsInterfaceImpl(core, context) {
			net::url url = net::parse(utf8::cvt<std::string>(context), 80);
			std::wstring path = core->expand_path(DEFAULT_CACHE_PATH);
			if (!file_helpers::checks::is_directory(path)) {
				if (file_helpers::checks::exists(path)) 
					throw new settings_exception(_T("Cache path not found: ") + path);
				boost::filesystem::create_directories(path);
				if (!file_helpers::checks::is_directory(path))
					throw new settings_exception(_T("Cache path not found: ") + path);
			}
			boost::filesystem::path wp(path);
			wp /= _T("cached.ini");
			std::ofstream os(utf8::cvt<std::string>(wp.string()).c_str());
			std::string error;
			if (!http::client::download(url.protocol, url.host, url.path, os, error)) {
				os.close();
				get_logger()->error(_T("settings"),__FILE__, __LINE__, _T("Failed to download settings: ") + utf8::cvt<std::wstring>(error));
			}
			os.close();
			if (!boost::filesystem::is_regular_file(wp)) {
				throw new settings_exception(_T("Failed to find cached settings: ") + wp.wstring());
			}
			add_child(_T("ini://") + wp.wstring());
		}
		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual SettingsInterfaceImpl* create_new_context(std::wstring context) {
			return new settings_http(get_core(), context);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual std::wstring get_real_string(settings_core::key_path_type key) {
			throw KeyNotFoundException(key);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual int get_real_int(settings_core::key_path_type key) {
			throw KeyNotFoundException(key);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual bool get_real_bool(settings_core::key_path_type key) {
			throw KeyNotFoundException(key);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Check if a key exists
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return true/false if the key exists.
		///
		/// @author mickem
		virtual bool has_real_key(settings_core::key_path_type key) {
			return false;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) {
			get_logger()->error(_T("settings"),__FILE__, __LINE__, std::wstring(_T("Cant save over HTTP: ") + key.first + _T(".") + key.second));
		}

		virtual void set_real_path(std::wstring path) {
			get_logger()->error(_T("settings"),__FILE__, __LINE__, std::wstring(_T("Cant save over HTTP: ") + path));
		}
		virtual void remove_real_value(settings_core::key_path_type key) {
			get_logger()->error(_T("settings"),__FILE__, __LINE__, std::wstring(_T("Cant save over HTTP")));
		}
		virtual void remove_real_path(std::wstring path) {
			get_logger()->error(_T("settings"),__FILE__, __LINE__, std::wstring(_T("Cant save over HTTP")));
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get all (sub) sections (given a path).
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_sections(std::wstring path, string_list &list) {
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys given a path/section.
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_keys(std::wstring path, string_list &list) {
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
		}

		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}

	private:

		std::wstring get_file_name() {
			if (url_.empty()) {
				url_ = get_file_from_context();
				get_logger()->debug(_T("settings"),__FILE__, __LINE__, _T("Reading INI settings from: ") + url_);
			}
			return url_;
		}
		bool file_exists() {
			return boost::filesystem::is_regular(get_file_name());
		}
		virtual std::wstring get_info() {
			return _T("HTTP settings: (") + context_ + _T(", ") + get_file_name() + _T(")");
		}

	};
}
