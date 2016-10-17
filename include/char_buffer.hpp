#pragma once

#include <buffer.hpp>
#include <string>

//typedef buffer<TCHAR> char_buffer;

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