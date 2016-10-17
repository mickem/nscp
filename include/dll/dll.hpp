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