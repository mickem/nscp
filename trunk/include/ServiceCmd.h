/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <sstream>

namespace serviceControll {
	class SCException {
	public:
		std::string error_;
		SCException(std::string error) : error_(error) {
		}
		SCException(std::string error, int code) : error_(error) {
			std::stringstream ss;
			ss << ": ";
			ss << code;
			error += ss.str();
		}
	};
	void Install(LPCTSTR,LPCTSTR,LPCTSTR,DWORD=SERVICE_WIN32_OWN_PROCESS);
	void ModifyServiceType(LPCTSTR szName, DWORD dwServiceType);
	void Uninstall(std::string);
	void Start(std::string);
	void Stop(std::string);
	void SetDescription(std::string,std::string);
	DWORD GetServiceType(LPCTSTR szName);
}