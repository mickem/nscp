/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <sstream>
#include <Windows.h>
#include <unicode_char.hpp>

namespace serviceControll {
	class SCException {
	public:
		std::string error_;
		SCException(std::string error) : error_(error) {}
		SCException(std::string error, int code) : error_(error) {
			std::stringstream ss;
			ss << ": ";
			ss << code;
			error += ss.str();
		}
	};
	void Install(std::wstring, std::wstring, std::wstring, DWORD = SERVICE_WIN32_OWN_PROCESS, std::wstring args = std::wstring(), std::wstring exe = std::wstring());
	void ModifyServiceType(LPCTSTR szName, DWORD dwServiceType);
	void Uninstall(std::wstring);
	void Start(std::wstring);
	bool isStarted(std::wstring);
	bool isInstalled(std::wstring name);
	void Stop(std::wstring);
	void StopNoWait(std::wstring);
	void SetDescription(std::wstring, std::wstring);
	DWORD GetServiceType(LPCTSTR szName);
	std::wstring get_exe_path(std::wstring svc_name);
}