#pragma once
#include <NSCAPI.h>

class OldCheckDisk {
public:
	static NSCAPI::nagiosReturn CheckFile(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf, bool show_errors);
	static NSCAPI::nagiosReturn CheckFile2(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf, bool show_errors);

};
