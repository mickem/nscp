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

#include <sstream>
#include <Windows.h>

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