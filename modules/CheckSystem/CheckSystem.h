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

#include <pdh.hpp>
#include "PDHCollector.h"
#include <CheckMemory.h>

class CheckSystem : public nscapi::impl::simple_plugin {
private:
	CheckMemory memoryChecker;
	PDHCollector pdh_collector;

	typedef std::map<std::string,std::string> counter_map_type;
	counter_map_type counters;

public:
	std::map<DWORD,std::string> lookups_;


public:
	CheckSystem() {}
	virtual ~CheckSystem() {}

	virtual bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	virtual bool unloadModule();

	NSCAPI::nagiosReturn commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);

	NSCAPI::nagiosReturn check_service(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn check_memory(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn check_pdh(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn check_process(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn check_cpu(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn check_uptime(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);

};
