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
#include <unicode_char.hpp>
#include <string>
#include <strEx.h>
#include <vector>

#ifdef WIN32
#include <error_impl_w32.hpp>
#else
#include <error_impl_unix.hpp>
#endif

class nscp_exception : public std::exception {
private:
	std::string error;
public:
	nscp_exception(std::string error) : error(error) {};
	~nscp_exception() throw() {}

	const char* what() const throw() {
		return error.c_str();
	}
	std::string reason() const throw() {
		return error;
	}
};

