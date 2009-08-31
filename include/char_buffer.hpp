#pragma once

#include <buffer.hpp>
#include <string>

//typedef buffer<TCHAR> char_buffer;

class char_buffer : public buffer<TCHAR> {
public:
	char_buffer(std::wstring str) : buffer<TCHAR>(str.length()+2) {
		wcsncpy_s(unsafe_get_buffer(), str.length()+2, str.c_str(), str.length());
	}
	char_buffer(unsigned int len) : buffer<TCHAR>(len) {}
	char_buffer() : buffer<TCHAR>() {}
	void zero() {
		if (length() > 1)
			ZeroMemory(unsafe_get_buffer(), length());
	}
};
