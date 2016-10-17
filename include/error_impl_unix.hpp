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
#include <cstdarg>
#include <unicode_char.hpp>
#include <string>

namespace error {
	class format {
	public:
		static std::string from_system(int dwError) {
			char buf [1024];
			::strerror_r(dwError, buf, sizeof (buf));
            return buf;
		}
		static std::string from_module(std::wstring module, unsigned long dwError) {
			return "ERROR TODO";
		}
		static std::string from_module(std::wstring module, unsigned long dwError, unsigned long *arguments) {
			return "ERROR TODO";
		}
		class message {
		public:
			static std::string from_module(std::wstring module, unsigned long dwError) {
				return "ERROR TODO";
			}

			static std::string from_module_x64(std::wstring module, unsigned long dwError, wchar_t* argList[], unsigned long argCount) {
				return "ERROR TODO";
			}
		private:

			static std::string from_module_wrapper(std::wstring module, unsigned long dwError, ...) {
				return "ERROR TODO";
			}

			static std::string __from_module(std::wstring module, unsigned long dwError, va_list *arguments) {
				return "ERROR TODO";
			}
		public:
			static std::string from_system(unsigned long dwError, unsigned long *arguments) {
				return "ERROR TODO";
			}
		};
	};
	class lookup {
	public:
		static std::string last_error(int dwLastError = -1) {
			return ::error::format::from_system(dwLastError);
		}
	};
}
