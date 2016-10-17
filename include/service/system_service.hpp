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