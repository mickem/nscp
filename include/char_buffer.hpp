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

#include <buffer.hpp>
#include <string>

namespace hlp {
	class tchar_buffer : public hlp::buffer<wchar_t> {
	public:
		tchar_buffer(std::wstring str) : hlp::buffer<wchar_t>(str.length()+2) {
			wcsncpy(get(), str.c_str(), str.length());
		}
		tchar_buffer(std::size_t len) : buffer<wchar_t>(len) {}
		void zero() {
			if (size() > 1)
				memset(get(), 0, size());
		}
	};

	class char_buffer : public buffer<char> {
	public:
		char_buffer(std::string str) : buffer<char>(str.length()+2) {
			strncpy(get(), str.c_str(), str.length());
		}
		char_buffer(unsigned int len) : buffer<char>(len) {}
		void zero() {
			if (size() > 1)
				memset(get(), 0, size());
		}
	};

}