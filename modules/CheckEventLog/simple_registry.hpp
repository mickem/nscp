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

#include <error/error.hpp>

namespace simple_registry {
	class registry_exception {
		std::string what_;
	public:
		registry_exception(std::string what) : what_(what) {}
		registry_exception(std::wstring path, std::string what) : what_(utf8::cvt<std::string>(path) + ": " + what) {}
		registry_exception(std::wstring path, std::wstring key, std::string what) : what_(utf8::cvt<std::string>(path) + "." + utf8::cvt<std::string>(key) + ": " + what) {}
		std::string reason() {
			return what_;
		}
	};
	class registry_key {
		HKEY hKey_;
		std::wstring path_;
		BYTE *bData_;
		TCHAR *buffer_;
	public:
		registry_key(HKEY hRootKey, std::wstring path) : path_(path), hKey_(NULL), bData_(NULL), buffer_(NULL) {
			LONG lRet = ERROR_SUCCESS;
			if (lRet = RegOpenKeyEx(hRootKey, path.c_str(), 0, KEY_QUERY_VALUE | KEY_READ, &hKey_) != ERROR_SUCCESS)
				throw registry_exception(path, "Failed to open key: " + error::format::from_system(lRet));
		}
		~registry_key() {
			if (hKey_ != NULL)
				RegCloseKey(hKey_);
			delete[] bData_;
			delete[] buffer_;
		}
		std::wstring get_string(std::wstring key, DWORD buffer_length = 2048) {
			DWORD type;
			std::wstring ret;
			DWORD cbData = buffer_length;
			delete[] bData_;
			bData_ = new BYTE[cbData + 2];
			// TODO: add get size here !
			LONG lRet = RegQueryValueEx(hKey_, key.c_str(), NULL, &type, bData_, &cbData);
			if (lRet != ERROR_SUCCESS)
				throw registry_exception(path_, key, "Failed to get value: " + error::format::from_system(lRet));
			if (cbData >= buffer_length || cbData < 0)
				throw registry_exception(path_, key, "Failed to get value: buffer to small");
			bData_[cbData] = 0;
			if (type == REG_SZ) {
				ret = reinterpret_cast<LPCTSTR>(bData_);
			} else if (type == REG_EXPAND_SZ) {
				std::wstring s = reinterpret_cast<LPCTSTR>(bData_);
				delete[] buffer_;
				buffer_ = new TCHAR[buffer_length + 1];
				DWORD expRet = ExpandEnvironmentStrings(s.c_str(), buffer_, buffer_length);
				if (expRet >= buffer_length)
					throw registry_exception(path_, key, "Buffer to small (expand)");
				else
					ret = buffer_;
			} else {
				throw registry_exception(path_, key, "Unknown type (not a string)");
			}
			return ret;
		}
		DWORD get_int(std::wstring key) {
			DWORD type;
			DWORD cbData = sizeof(DWORD);
			DWORD ret = 0;
			LONG lRet = RegQueryValueEx(hKey_, key.c_str(), NULL, &type, reinterpret_cast<LPBYTE>(&ret), &cbData);
			if (lRet != ERROR_SUCCESS)
				throw registry_exception(path_, key, "Failed to get value: " + error::format::from_system(lRet));
			if (type != REG_DWORD)
				throw registry_exception(path_, key, "Unknown type (not a DWORD)");
			return ret;
		}

		std::list<std::wstring> get_keys(DWORD buffer_length = 2048) {
			std::list<std::wstring> ret;
			DWORD cSubKeys = 0;
			DWORD cMaxKeyLen;
			// Get the class name and the value count.
			LONG lRet = RegQueryInfoKey(hKey_, NULL, NULL, NULL, &cSubKeys, &cMaxKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);
			if (lRet != ERROR_SUCCESS)
				throw registry_exception(path_, "Failed to query key info: " + error::format::from_system(lRet));
			if (cSubKeys == 0)
				return ret;
			delete[] buffer_;
			buffer_ = new TCHAR[cMaxKeyLen + 20];
			for (unsigned int i = 0; i < cSubKeys; i++) {
				lRet = RegEnumKey(hKey_, i, buffer_, cMaxKeyLen + 10);
				if (lRet != ERROR_SUCCESS) {
					throw registry_exception(path_, "Failed to enumerate: " + error::lookup::last_error(lRet));
				}
				std::wstring str = buffer_;
				ret.push_back(str);
			}
			return ret;
		}

		static std::wstring get_string(HKEY hKey, std::wstring path, std::wstring key) {
			registry_key reg(hKey, path);
			return reg.get_string(key);
		}
	};
}