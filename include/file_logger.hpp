#pragma once

#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>


namespace simple_file {

#ifndef CSIDL_COMMON_APPDATA 
#define CSIDL_COMMON_APPDATA 0x0023 
#define CSIDL_LOCAL_APPDATA  0x001c
#endif
	typedef BOOL (WINAPI *fnSHGetSpecialFolderPath)(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate);
		static BOOL WINAPI _SHGetSpecialFolderPath(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate) {
			static fnSHGetSpecialFolderPath __SHGetSpecialFolderPath = NULL;
			if (!__SHGetSpecialFolderPath) {
				HMODULE hDLL = LoadLibrary(_T("shell32.dll"));
				if (hDLL != NULL) { 
					__SHGetSpecialFolderPath = (fnSHGetSpecialFolderPath)GetProcAddress(hDLL,"SHGetSpecialFolderPathW");
				}
			}
			if(__SHGetSpecialFolderPath)
				return __SHGetSpecialFolderPath(hwndOwner, lpszPath, nFolder, fCreate);
			return FALSE;
		}


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
			if (file_.empty()) {
				std::wstring path = getFolder() + _T("\\") + pathname;
				if (!directoryExists(path)) {
					if (_wmkdir(path.c_str()) != 0)
						return _T("");
				}
				file_ = path + _T("\\") + filename;
			}
			return file_;
		}
	private:
		inline std::wstring getFolder() {
			TCHAR buf[MAX_PATH+1];
			if (!_SHGetSpecialFolderPath(NULL, buf, CSIDL_LOCAL_APPDATA, FALSE)) {
				return _T("") + error::lookup::last_error();
			}
			return buf;
		}
		bool directoryExists(std::wstring path) {
			DWORD dwAtt = ::GetFileAttributes(path.c_str());
			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
				return false;
			} else if ((dwAtt&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY) {
				return true;
			}
			return false;
		}

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
					wcsncpy(buffer, 64, _T("???"));
				else
					buffer[len] = 0;
			} else {
				wcsncpy(buffer, 64, _T("<unknown time>"));
			}
			std::wstring logline = std::wstring(buffer) + _T(": ") + category + _T(":") + std::wstring(file) + _T(":") + strEx::itos(line) +_T(": ") + message + _T("\r\n");
			if (!writeEntry(logline)) {
				std::wcerr << _T("Failed to write: ") << logline;
			}
		}
	};
}