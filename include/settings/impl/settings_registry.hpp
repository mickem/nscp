/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <settings/settings_core.hpp>
#include <config.h>

#include <error/error.hpp>

#include <str/xtos.hpp>

#include <handle.hpp>
#include <buffer.hpp>

#include <boost/algorithm/string.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#define BUFF_LEN 4096

namespace settings {
	struct registry_closer {
		static void close(HKEY hKey) {
			RegCloseKey(hKey);
		}
	};
	typedef hlp::handle<HKEY, registry_closer> reg_handle;
	typedef hlp::buffer<wchar_t, const BYTE*> reg_buffer;
	class REGSettings : public settings::settings_interface_impl {
	private:
		struct reg_key {
			HKEY hKey;
			std::wstring path;

			static reg_key from_context(std::string context) {
				net::url url = net::parse(context);
				std::string file = url.path;
				boost::replace_all(file, "/", "\\");
				HKEY hKey;
				std::wstring path;
				if (url.host == "HKEY_LOCAL_MACHINE" || url.host == "LOCAL_MACHINE" || url.host == "HKLM")
					hKey = HKEY_LOCAL_MACHINE;
				else if (url.host == "HKEY_CURRENT_USER" || url.host == "CURRENT_USER" || url.host == "HKCU")
					hKey = HKEY_CURRENT_USER;
				else
					hKey = NS_HKEY_ROOT;
				if (!file.empty()) {
					if (file[0] == '\\')
						file = file.substr(1);
					path = utf8::cvt<std::wstring>(file);
				} else
					path = NS_REG_ROOT;
				return reg_key(hKey, path);
			}
			reg_key(HKEY hKey, std::wstring path) : hKey(hKey), path(path) {}
			std::string to_string() const {
				return utf8::cvt<std::string>(path);
			}
			reg_key get_subkey(std::wstring sub_path) const {
				return reg_key(hKey, path + boost::replace_all_copy(sub_path, L"/", L"\\"));
			}
			reg_key get_subkey(std::string sub_path) const {
				return get_subkey(utf8::cvt<std::wstring>(sub_path));
			}
		};

		struct write_reg_key {
			reg_handle hTemp;
			const reg_key &source;
			write_reg_key(const reg_key &source) : source(source) {
				open();
			}
			void open() {
				DWORD err = RegCreateKeyEx(source.hKey, source.path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, hTemp.ref(), NULL);
				if (err != ERROR_SUCCESS) {
					throw settings_exception(__FILE__, __LINE__, "Failed to create " + source.to_string() + ": " + error::lookup::last_error(err));
				}
			}
			HKEY operator*() {
				if (!hTemp)
					throw settings_exception(__FILE__, __LINE__, "No valid handle: " + source.to_string());
				return hTemp;
			}
			void setValueEx(const std::string &key, DWORD type, const BYTE *lpData, DWORD size) const {
				if (!hTemp)
					throw settings_exception(__FILE__, __LINE__, "No valid handle: " + source.to_string());
				std::wstring wkey = utf8::cvt<std::wstring>(key);
				DWORD err = RegSetValueExW(hTemp.get(), wkey.c_str(), 0, type, lpData, size);
				if (err != ERROR_SUCCESS) {
					throw settings_exception(__FILE__, __LINE__, "Failed to write string " + source.to_string() + "." + key + ": " + error::lookup::last_error(err));
				}
			}
			inline void setValueEx(const std::string &key, DWORD type, const reg_buffer &buffer) const {
				setValueEx(key, type, buffer.get(), buffer.size_in_bytes());
			}
			inline void setValueEx(const std::string &key, DWORD type, const DWORD value) const {
				setValueEx(key, type, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
			}
			inline void setValueEx(const std::string &key, DWORD type, const std::string &value) const {
				std::wstring wvalue = utf8::cvt<std::wstring>(value);
				reg_buffer buffer(wvalue.size() + 1, wvalue.c_str());
				setValueEx(key, type, buffer);
			}
		};

		reg_key root;

	public:
		REGSettings(settings::settings_core *core, std::string alias, std::string context) : settings::settings_interface_impl(core, alias, context), root(reg_key::from_context(context)) {
			std::list<std::string> list;
			reg_key path = get_reg_key("/includes");
			getValues_(path, list);
			get_core()->register_path(999, "/includes", "INCLUDED FILES", "Files to be included in the configuration", false, false);
			BOOST_FOREACH(const std::string &s, list) {
				op_string child = getString_(path, s);
				if (child) {
					get_core()->register_key(999, "/includes", s, "INCLUDED FILE", *child, *child, false, false);
					add_child_unsafe(*child, *child);
				}
			}
		}

		virtual ~REGSettings(void) {}

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_string get_real_string(settings_core::key_path_type key) {
			return getString_(get_reg_key(key.first), key.second);
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
		virtual bool has_real_path(std::string path) {
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
			setString_(get_reg_key(key), key.second, value.get_string());
		}
		virtual void set_real_path(std::string path) {
			// NOT Supported (and not needed) so silently ignored!
		}

		virtual void remove_real_value(settings_core::key_path_type key) {
			// NOT Supported
		}
		virtual void remove_real_path(std::string path) {
			// NOT Supported
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
		virtual void get_real_sections(std::string path, string_list &list) {
			getSubKeys_(get_reg_key(path), list);
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
		virtual void get_real_keys(std::string path, string_list &list) {
			getValues_(get_reg_key(path), list);
		}
	private:
		reg_key get_reg_key(const settings_core::key_path_type key) const {
			return root.get_subkey(key.first);
		}
		reg_key get_reg_key(const std::string path) const {
			return root.get_subkey(path);
		}

		static void setString_(const reg_key &path, std::string key, std::string value) {
			write_reg_key open_key(path);
			open_key.setValueEx(key, REG_SZ, value);
		}

		static op_string getString_(reg_key path, std::string key) {
			reg_handle hTemp;
			if (RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_QUERY_VALUE, hTemp.ref()) != ERROR_SUCCESS)
				return op_string();
			DWORD type;
			const DWORD data_length = 2048;
			DWORD cbData = data_length;
			hlp::buffer<BYTE> bData(cbData);
			LONG lRet = RegQueryValueEx(hTemp, utf8::cvt<std::wstring>(key).c_str(), NULL, &type, bData.get(), &cbData);
			if (lRet == ERROR_SUCCESS) {
				if (type == REG_SZ || type == REG_EXPAND_SZ) {
					if (cbData == 0)
						return "";
					if (cbData < data_length - 1) {
						bData[cbData] = 0;
						return utf8::cvt<std::string>(std::wstring(reinterpret_cast<TCHAR*>(bData.get())));
					}
					throw settings_exception(__FILE__, __LINE__, "String to long: " + path.to_string());
				} else if (type == REG_DWORD) {
					DWORD dw = *(reinterpret_cast<DWORD*>(bData.get()));
					return str::xtos(dw);
				}
				throw settings_exception(__FILE__, __LINE__, "Unsupported key type: " + path.to_string());
			} else if (lRet == ERROR_FILE_NOT_FOUND)
				return op_string();
			throw settings_exception(__FILE__, __LINE__, "Failed to open key: " + path.to_string() + ": " + error::lookup::last_error(lRet));
		}
		static void getValues_(reg_key path, string_list &list) {
			LONG bRet;
			reg_handle hTemp;
			if ((bRet = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, hTemp.ref())) != ERROR_SUCCESS)
				return;
			DWORD cValues = 0;
			DWORD cMaxValLen = 0;
			// Get the class name and the value count.
			bRet = RegQueryInfoKey(hTemp, NULL, NULL, NULL, NULL, NULL, NULL, &cValues, &cMaxValLen, NULL, NULL, NULL);
			cMaxValLen++;
			if ((bRet == ERROR_SUCCESS) && (cValues > 0)) {
				hlp::buffer<wchar_t> lpValueName(cMaxValLen + 1);
				for (unsigned int i = 0; i < cValues; i++) {
					DWORD len = cMaxValLen;
					bRet = RegEnumValue(hTemp, i, lpValueName.get(), &len, NULL, NULL, NULL, NULL);
					if (bRet != ERROR_SUCCESS)
						throw settings_exception(__FILE__, __LINE__, "Failed to enumerate: " + path.to_string() + ": " + error::lookup::last_error());
					list.push_back(utf8::cvt<std::string>(std::wstring(lpValueName)));
				}
			}
		}
		static bool has_key(reg_key path) {
			reg_handle hTemp;
			DWORD status = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, hTemp.ref());
			return status == ERROR_SUCCESS;
		}
		static void getSubKeys_(reg_key path, string_list &list) {
			LONG bRet;
			reg_handle hTemp;
			if ((bRet = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, hTemp.ref())) != ERROR_SUCCESS)
				return;
			DWORD cSubKeys = 0;
			DWORD cMaxKeyLen;
			// Get the class name and the value count.
			bRet = RegQueryInfoKey(hTemp, NULL, NULL, NULL, &cSubKeys, &cMaxKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);
			cMaxKeyLen++;
			if ((bRet == ERROR_SUCCESS) && (cSubKeys > 0)) {
				hlp::buffer<wchar_t> lpValueName(cMaxKeyLen + 1);
				for (unsigned int i = 0; i < cSubKeys; i++) {
					DWORD len = cMaxKeyLen;
					bRet = RegEnumKey(hTemp, i, lpValueName, len);
					if (bRet != ERROR_SUCCESS)
						throw settings_exception(__FILE__, __LINE__, "Failed to enumerate: " + path.to_string() + ": " + error::lookup::last_error());
					list.push_back(utf8::cvt<std::string>(std::wstring(lpValueName)));
				}
			}
		}
		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}

		virtual std::string get_info() {
			return "Registry settings: (" + context_ + "," + root.to_string() + ")";
		}
		virtual std::string get_type() { return "registry"; }

	public:
		static bool context_exists(settings::settings_core *, std::string key) {
			return has_key(reg_key::from_context(key));
		}
		virtual void real_clear_cache() {}
		void ensure_exists() {
			write_reg_key open_key(root);
		}
	};
}