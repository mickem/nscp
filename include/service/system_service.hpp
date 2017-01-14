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
#include <string>
#ifdef _WIN32
#include <utf8.hpp>
#endif

namespace service_helper {
	class service_exception : public std::exception {
		std::string what_;
	public:
		service_exception(std::string what) : what_(what) {
#ifdef _WIN32
			OutputDebugString(utf8::cvt<std::wstring>(std::string("ERROR:") + what).c_str());
#endif
		}
		virtual ~service_exception() throw() {}
		virtual const char* what() const throw() {
			return what_.c_str();
		}
	};
}

#ifdef _WIN32
#include <service/win32_service.hpp>
#else
#include <service/unix_service.hpp>
#endif

namespace service_helper {
	template<class T>
	class impl {
	public:
#ifdef _WIN32
		typedef service_helper_impl::win32_service<T> system_service;
#else
		typedef service_helper_impl::unix_service<T> system_service;
#endif
	};
}