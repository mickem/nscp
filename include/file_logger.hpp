#pragma once

#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

#include <file_helpers.hpp>


namespace simple_file {

	class file_appender {
		std::wstring file_;
		std::wstring path_;
		std::wstring filename_;
	public:
		file_appender(std::wstring path, std::wstring filename) : path_(path), filename_(filename) {}
		file_appender() {}

		void set_file(std::wstring path, std::wstring filename) {
			path_ = path;
			filename_ = filename;
		}

		inline std::wstring getFileName() {
			if (file_.empty())
				return getFileName(path_, filename_);
			return file_;
		}
		std::wstring getFileName(std::wstring pathname, std::wstring filename) {
			if (file_.empty()) 
				file_ = file_helpers::folders::get_local_appdata_file(pathname, filename);
			return file_;
		}
	private:


		HANDLE openAppendOrNew(std::wstring file) {
			DWORD numberOfBytesWritten = 0;
			HANDLE hFile = ::CreateFile(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile == INVALID_HANDLE_VALUE) {
				hFile = ::CreateFile(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile != INVALID_HANDLE_VALUE) {
					WORD wBOM = 0xFEFF;
					::WriteFile(hFile, &wBOM, sizeof(WORD), &numberOfBytesWritten, NULL);
				} else {
					int x = 5;
				}
			}
			return hFile;
		}
	public:

		bool writeEntry(std::wstring line) {
			DWORD numberOfBytesWritten;
			HANDLE hFile = openAppendOrNew(getFileName());
			if (hFile == INVALID_HANDLE_VALUE) {
				return false;
			}
			if (::SetFilePointer(hFile, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
				// Ingore this error!
			}
			::WriteFile(hFile, line.c_str(), (static_cast<DWORD>(line.length()))*(sizeof(TCHAR)), &numberOfBytesWritten, NULL);
			::CloseHandle(hFile);
			return true;
		}
	};
}

namespace logging {
	class file_logger : public simple_file::file_appender {
		std::wstring datemask_;
	public:
		file_logger(std::wstring path, std::wstring filename) : file_appender(path, filename), datemask_(_T("%Y-%m-%d %H:%M:%S")) {
			_tzset();
		}
		void log(const std::wstring category, const wchar_t* file, const int line, const wchar_t* message) {
			TCHAR buffer[65];
			__time64_t ltime;
			_time64( &ltime );
			struct tm *today = _localtime64( &ltime );
			if (today) {
				size_t len = wcsftime(buffer, 63, datemask_.c_str(), today);
				if ((len < 1)||(len > 64))
					wcsncpy_s(buffer, 64, _T("???"), 63);
				else
					buffer[len] = 0;
			} else {
				wcsncpy_s(buffer, 64, _T("<unknown time>"), 63);
			}
			std::wstring logline = std::wstring(buffer) + _T(": ") + category + _T(":") + std::wstring(file) + _T(":") + strEx::itos(line) +_T(": ") + message + _T("\r\n");
			if (!writeEntry(logline)) {
				std::wcerr << _T("Failed to write: ") << logline;
			}
		}
	};
}