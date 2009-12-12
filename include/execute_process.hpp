/**************************************************************************
*   Copyright (C) 2004-2008 by Michael Medin <michael@medin.name>         *
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

namespace process {
	class Exception {
		std::wstring error_;
	public:
		Exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};

	class exec_arguments {
	public:
		exec_arguments(std::wstring root_path_, std::wstring command_, unsigned int timeout_)
			: root_path(root_path_)
			, command(command_)
			, timeout(timeout_) 
		{}

		std::wstring root_path;
		std::wstring command;
		unsigned int timeout;
	};
}

#ifdef _WIN32
#include "execute_process_w32.hpp"
#else
#include "execute_process_unix.hpp"
#endif

