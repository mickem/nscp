#pragma once

namespace file_helpers {
	class checks {
	public:
		static bool is_directory(DWORD dwAttr) {
			if (dwAttr == INVALID_FILE_ATTRIBUTES) {
				return false;
			} else if ((dwAttr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY) {
				return true;
			}
			return false;
			//return ((dwAttr != INVALID_FILE_ATTRIBUTES) && ((dwAttr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
		}

		static bool is_directory(std::wstring path) {
			return is_directory(::GetFileAttributes(path.c_str()));
		}
		static bool is_file(std::wstring path) {
			DWORD dwAtt = ::GetFileAttributes(path.c_str());
			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
				return false;
			} else if ((dwAtt&FILE_ATTRIBUTE_NORMAL)==FILE_ATTRIBUTE_NORMAL) {
				return true;
			}
			return false;
		}
		static bool exists(std::wstring path) {
			DWORD dwAtt = ::GetFileAttributes(path.c_str());
			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
				return false;
			}
			return true;
		}
	};
	class meta {
	public:
		static std::wstring get_path(std::wstring file) {
			std::wstring::size_type pos = file.find_last_of('\\');
			if (pos == std::wstring::npos) {
				return file;
			}
			return file.substr(0, pos);
		}
		static std::wstring get_filename(std::wstring file) {
			std::wstring::size_type pos = file.find_last_of('\\');
			if (pos == std::wstring::npos || ++pos == std::wstring::npos) {
				return _T("");
			}
			return file.substr(pos);
		}
	};

	class patterns {
	public:
		typedef std::pair<std::wstring,std::wstring> pattern_type;

		static pattern_type split_pattern(std::wstring path) {
			std::wstring baseDir;
			if (file_helpers::checks::exists(path)) {
				return pattern_type(path, _T(""));
			}
			std::wstring::size_type pos = path.find_last_of('\\');
			if (pos == std::wstring::npos) {
				pattern_type(path, _T("*.*"));
			}
			return pattern_type(path.substr(0, pos), path.substr(pos+1));
		}
		static std::wstring combine_pattern(pattern_type pattern) {
			return pattern.first + _T("\\") + pattern.second;
		}

		static pattern_type split_path_ex(std::wstring path) {
			std::wstring baseDir;
			if (file_helpers::checks::is_directory(path)) {
				return pattern_type(path, _T(""));
			}
			std::wstring::size_type pos = path.find_last_of('\\');
			if (pos == std::wstring::npos) {
				pattern_type(path, _T("*.*"));
			}
			//NSC_DEBUG_MSG_STD(_T("Looking for: path: ") + path.substr(0, pos) + _T(", pattern: ") + path.substr(pos+1));
			return pattern_type(path.substr(0, pos), path.substr(pos+1));
		}



	};

	class folders {
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
	public:
		static inline std::wstring get_folder(int folderId) {
			TCHAR buf[MAX_PATH+1];
			if (!_SHGetSpecialFolderPath(NULL, buf, folderId, FALSE)) {
				return _T("");
			}
			return buf;
		}

		static std::wstring get_local_appdata() {
			return get_folder(CSIDL_LOCAL_APPDATA);
		}

		static std::wstring get_local_appdata_folder(std::wstring pathname) {
			return get_subfolder(file_helpers::folders::get_local_appdata(), pathname);
		}
		static std::wstring get_local_appdata_file(std::wstring pathname, std::wstring filename) {
			return get_local_appdata_folder(pathname) + _T("\\") + filename;
		}

		static std::wstring get_subfolder(std::wstring root, std::wstring folder) {
			std::wstring path = root + _T("\\") + folder;
			if (!file_helpers::checks::exists(path)) {
				if (_wmkdir(path.c_str()) != 0)
					return _T("");
			}
			return path;
		}


	};

}
