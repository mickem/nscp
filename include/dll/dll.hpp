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

namespace dll {
	class dll_exception : public std::exception {
		std::string what_;
	public:
		dll_exception(std::string what) : what_(what) {}
		~dll_exception() throw() {}
		const char* what() const throw() {
			return what_.c_str();
		}
	};
}

#ifdef WIN32
#include <dll/impl_w32.hpp>
#else
#include <dll/impl_unix.hpp>
#endif
namespace dll {
#ifdef WIN32
	typedef ::dll::win32::impl dll_impl;
#else
	typedef dll::iunix::impl dll_impl;
#endif
}