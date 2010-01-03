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
#include "stdafx.h"
#include "Scheduler.h"
#include <strEx.h>
#include <time.h>
#include <utils.h>

Scheduler gInstance;


bool Scheduler::loadModule(NSCAPI::moduleLoadMode mode) {
	if (mode == NSCAPI::normalStart) {
		scheduler_.start();
	}
	add_schedule(_T("test 001"));
	add_schedule(_T("test 002"));
	return true;
}


void Scheduler::add_schedule(std::wstring command) {
	scheduler::target item;
	item.command = command;
	item.duration = boost::posix_time::time_duration(0,0,5);
	std::wcout << _T("*** DURATION ") << item.duration << _T(" ***") << std::endl;
	scheduler_.add_task(item);
}
bool Scheduler::unloadModule() {
	scheduler_.stop();
	return true;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gInstance);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
