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
NSC_WRAPPERS_MAIN();
#include <config.h>
#include <strEx.h>

#include "simple_scheduler.hpp"


class Scheduler {
private:
	scheduler::simple_scheduler scheduler_;


public:
	Scheduler() {}
	virtual ~Scheduler() {}
	// Module calls
	bool loadModule(NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	void add_schedule(std::wstring command, scheduler::target def);
	scheduler::target read_schedule(std::wstring path, scheduler::target def);
	scheduler::target read_schedule(std::wstring path);

	std::wstring getModuleName() {
		return _T("Scheduler");
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 3, 0 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("A scheduler which schedules checks at regular intervals");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
};