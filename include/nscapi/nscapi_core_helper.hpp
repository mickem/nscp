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

#include <string>
#include <list>

namespace nscapi {
	namespace core_helper {
		NSCAPI::nagiosReturn simple_query(const std::wstring command, const std::list<std::wstring> & argument, std::wstring & message, std::wstring & perf);
		NSCAPI::nagiosReturn simple_query(const std::wstring command, const std::list<std::wstring> & argument, std::string & result);
		NSCAPI::nagiosReturn simple_query_from_nrpe(const std::wstring command, const std::wstring & buffer, std::wstring & message, std::wstring & perf);

		NSCAPI::nagiosReturn exec_simple_command(const std::wstring target, const std::wstring command, const std::list<std::wstring> &argument, std::list<std::wstring> & result);
		bool submit_simple_message(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn code, std::wstring & message, std::wstring & perf, std::wstring & response);
	};
};
