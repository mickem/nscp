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

#ifdef WIN32
#include <error/error_w32.hpp>
#ifndef FORMAT_MESSAGE_IGNORE_INSERTS
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200
#endif
#else
#include <string.h>
#include <cstdarg>
#endif

#include <string>

namespace error {
	class format {
	public:
#ifdef WIN32
		static std::string from_system(unsigned long dwError) {
			return win32::format_message(0, "", dwError);
		}
		static std::string from_module(std::string module, unsigned long dwError) {
			return win32::format_message(FORMAT_MESSAGE_IGNORE_INSERTS, module, dwError);
		}
		static std::string from_system(unsigned long dwError, unsigned long *arguments) {
			return win32::format_message(0, "", dwError, arguments);
		}
		static std::string from_module(std::string module, unsigned long dwError, unsigned long *arguments) {
			return win32::format_message(0, module, dwError, arguments);
		}
		class message {
		public:
			static std::string from_module(std::string module, unsigned long dwError) {
				return win32::format_message(FORMAT_MESSAGE_IGNORE_INSERTS, module, dwError);
			}
			static std::string from_system(unsigned long dwError, unsigned long *arguments) {
				return win32::format_message(0, "", dwError, arguments);
			}
		};
#else
    static std::string from_system(int dwError) {
      char buf [1024];
      ::strerror_r(dwError, buf, sizeof (buf));
      return buf;
    }
#endif
	};
	class lookup {
	public:
#ifdef WIN32
		static std::string last_error(unsigned long dwLastError = -1) {
			return win32::format_message(0, "", dwLastError == -1 ? win32::lookup() : dwLastError);
		}
#else
		static std::string last_error(int dwLastError = -1) {
			return ::error::format::from_system(dwLastError);
		}
#endif
	};
}
