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
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>

class CheckDisk : public nscapi::impl::simple_plugin  {
private:
	bool show_errors_;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDiscSize> PathContainer;
	typedef checkHolders::MagicCheckContainer<checkHolders::MaxMinPercentageBoundsDiskSize> DriveContainer;

public:
	CheckDisk();
	virtual ~CheckDisk();

	std::wstring get_filter(unsigned int drvType);

	// Check commands
	//NSCAPI::nagiosReturn check_filesize(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn check_files(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn check_drivesize(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf);
};
