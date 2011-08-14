#pragma once

#include <buffer.hpp>
#include <string>

//typedef buffer<TCHAR> char_buffer;

class tchar_buffer : public buffer<TCHAR> {
public:
	tchar_buffer(std::wstring str) : buffer<TCHAR>(str.length()+2) {
		wcsncpy(unsafe_get_buffer(), str.c_str(), str.length());
	}
	tchar_buffer(unsigned int len) : buffer<TCHAR>(len) {}
	tchar_buffer() : buffer<TCHAR>() {}
	void zero() {
		if (length() > 1)
			ZeroMemory(unsafe_get_buffer(), length());
	}
};

class char_buffer : public buffer<char> {
public:
	char_buffer(std::string str) : buffer<char>(str.length()+2) {
		strncpy(unsafe_get_buffer(), str.c_str(), str.length());
	}
	char_buffer(unsigned int len) : buffer<char>(len) {}
	char_buffer() : buffer<char>() {}
	void zero() {
		if (length() > 1)
			ZeroMemory(unsafe_get_buffer(), length());
	}
};

