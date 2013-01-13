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

//#define _WIN32_DCOM

#include <string>
#include <map>
#include <strEx.h>
#include <error.hpp>
#include <filter_framework.hpp>
//#include <taskschd.h>

#include "filter.hpp"

class TaskSched
{
public:
	class Exception {
		std::wstring message_;
	public:
		Exception(std::wstring str, HRESULT code) {
			message_ = str + _T(":") + error::format::from_system(code);
		}
		std::wstring getMessage() {
			return message_;
		}
	};

	void findAll(tasksched_filter::filter_result result, tasksched_filter::filter_argument args, tasksched_filter::filter_engine engine);

};
