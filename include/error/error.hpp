/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
